#include "fmp4-writer.h"
#include "mov-internal.h"
#include "file-writer.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

struct fmp4_writer_t
{
	struct mov_t mov;
	size_t mdat_size;
	int has_moov;

	uint32_t frag_interleave;
	uint32_t fragment_id; // start from 1
	uint32_t sn; // sample sn
};

static int fmp4_writer_save(fmp4_writer_t* writer);

static size_t fmp4_write_mvex(struct mov_t* mov)
{
	size_t size, i;
	uint64_t offset;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "mvex", 4);

	//size += fmp4_write_mehd(mov);
	for (i = 0; i < mov->track_count; i++)
	{
		mov->track = mov->tracks + i;
		size += mov_write_trex(mov);
	}
	//size += mov_write_leva(mov);

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static size_t fmp4_write_mfhd(struct mov_t* mov, uint32_t fragment)
{
	file_writer_wb32(mov->fp, 16); /* size */
	file_writer_write(mov->fp, "mfhd", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */
	file_writer_wb32(mov->fp, fragment); /* sequence_number */
	return 16;
}

static size_t fmp4_write_tfdt(struct mov_t* mov)
{
	uint8_t version;
	version = mov->track->samples[0].dts > INT32_MAX ? 1 : 0;
	file_writer_wb32(mov->fp, 0 == version ? 16 : 20); /* size */
	file_writer_write(mov->fp, "tfdt", 4);
	file_writer_w8(mov->fp, version); /* version */
	file_writer_wb24(mov->fp, 0); /* flags */
	if (1 == version)
		file_writer_wb64(mov->fp, mov->track->samples[0].dts); /* baseMediaDecodeTime */
	else
		file_writer_wb32(mov->fp, (uint32_t)mov->track->samples[0].dts); /* baseMediaDecodeTime */
	return 0 == version ? 16 : 20;
}

static size_t fmp4_write_traf(struct mov_t* mov)
{
	size_t i, size;
	uint64_t offset;
	uint32_t flags, first;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "traf", 4);

	mov->track->tfhd.flags = MOV_TFHD_FLAG_BASE_DATA_OFFSET | MOV_TFHD_FLAG_DEFAULT_FLAGS;
	// ISO/IEC 23009-1:2014(E) 6.3.4.2 General format type (p93)
	// The ¡®moof¡¯ boxes shall use movie-fragment relative addressing for media data that 
	// does not use external data references, the flag ¡®default-base-is-moof¡¯ shall be set, 
	// and data-offset shall be used, i.e. base-data-offset-present shall not be used.
	if (mov->flags & MOV_FLAG_SEGMENT)
	{
		mov->track->tfhd.flags &= ~MOV_TFHD_FLAG_BASE_DATA_OFFSET;
		mov->track->tfhd.flags |= MOV_TFHD_FLAG_DEFAULT_BASE_IS_MOOF;
	}
	mov->track->tfhd.base_data_offset = mov->moof_offset;
	mov->track->tfhd.sample_description_index = 1; // not set
	mov->track->tfhd.default_sample_duration = 0; // not set
	mov->track->tfhd.default_sample_size = 0; // not set
	mov->track->tfhd.default_sample_flags = MOV_AUDIO == mov->track->handler_type ? 0x02000000 : 0x01010000;
	if (0 == mov->track->sample_count)
		mov->track->tfhd.flags |= MOV_TFHD_FLAG_DURATION_IS_EMPTY;

	size += mov_write_tfhd(mov);
	// ISO/IEC 23009-1:2014(E) 6.3.4.2 General format type (p93)
	// Each ¡®traf¡¯ box shall contain a ¡®tfdt¡¯ box.
	size += fmp4_write_tfdt(mov);

	mov->track->offset = file_writer_tell(mov->fp) + 16; // trun data offset
	flags = MOV_TRUN_FLAG_DATA_OFFSET_PRESENT | MOV_TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT;
	flags |= MOV_TRUN_FLAG_SAMPLE_DURATION_PRESENT | MOV_TRUN_FLAG_SAMPLE_SIZE_PRESENT;
	flags |= MOV_TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT;
	first = 0x02000000;

	// cluster trun
	for (i = 0; i < mov->track->sample_count; i += mov->track->samples[i].samples_per_chunk)
	{
		assert(mov->track->samples[i].samples_per_chunk > 0);
//		assert(i == mov->track->samples[i].first_chunk);
//		assert(i + mov->track->samples[i].samples_per_chunk <= mov->track->sample_count);
		mov->track->samples[i].offset = file_writer_tell(mov->fp) + 16; // trun data offset
		size += mov_write_trun(mov, flags, first, i, mov->track->samples[i].samples_per_chunk);
	}

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static size_t fmp4_write_moof(struct mov_t* mov, uint32_t fragment)
{
	size_t size, i;
	uint64_t offset;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "moof", 4);

	size += fmp4_write_mfhd(mov, fragment);

	for (i = 0; i < mov->track_count; i++)
	{
		mov->track = mov->tracks + i;
		if (mov->track->sample_count > 0)
			size += fmp4_write_traf(mov);
	}

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static size_t fmp4_write_stbl(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "stbl", 4);

	size += mov_write_stsd(mov);
	size += mov_write_stts(mov, 0);
	size += mov_write_stsc(mov);
	size += mov_write_stsz(mov);
	size += mov_write_stco(mov, 0);

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static size_t fmp4_write_minf(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;
	const struct mov_track_t* track = mov->track;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "minf", 4);

	if (MOV_VIDEO == track->handler_type)
	{
		size += mov_write_vmhd(mov);
	}
	else if (MOV_AUDIO == track->handler_type)
	{
		size += mov_write_smhd(mov);
	}
	else
	{
		assert(0);
	}

	size += mov_write_dinf(mov);
	size += fmp4_write_stbl(mov);
	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static int fmp4_write_mdia(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "mdia", 4);

	size += mov_write_mdhd(mov);
	size += mov_write_hdlr(mov);
	size += fmp4_write_minf(mov);

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static size_t fmp4_write_trak(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "trak", 4);

	size += mov_write_tkhd(mov);
//	size += fmp4_write_tref(mov);
//	size += fmp4_write_edts(mov);
	size += fmp4_write_mdia(mov);

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static size_t fmp4_write_moov(struct mov_t* mov)
{
	size_t size;
	size_t i, count;
	uint64_t offset;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "moov", 4);

	size += mov_write_mvhd(mov);
//	size += fmp4_write_iods(mov);
	for (i = 0; i < mov->track_count; i++)
	{
		mov->track = mov->tracks + i;
		count = mov->track->sample_count;
		mov->track->sample_count = 0;
		size += fmp4_write_trak(mov);
		mov->track->sample_count = count; // restore sample count
	}

	size += fmp4_write_mvex(mov);
//  size += fmp4_write_udta(mov);
	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static size_t fmp4_write_sidx(struct mov_t* mov)
{
	size_t i;
	uint32_t duration;
	uint64_t earliest_presentation_time;
	struct mov_track_t* track;

	for (i = 0; i < mov->track_count; i++)
	{
		track = mov->tracks + i;
		if (track->sample_count > 0)
		{
			earliest_presentation_time = track->samples[0].pts;
			duration = (uint32_t)(track->samples[track->sample_count - 1].dts - track->samples[0].dts);
		}
		else
		{
			earliest_presentation_time = 0;
			duration = 0;
		}

		file_writer_wb32(mov->fp, 52); /* size */
		file_writer_write(mov->fp, "sidx", 4);
		file_writer_w8(mov->fp, 1); /* version */
		file_writer_wb24(mov->fp, 0); /* flags */

		file_writer_wb32(mov->fp, track->tkhd.track_ID); /* reference_ID */
		file_writer_wb32(mov->fp, track->mdhd.timescale); /* timescale */
		file_writer_wb64(mov->fp, earliest_presentation_time); /* earliest_presentation_time */
		file_writer_wb64(mov->fp, 52 * (mov->track_count - i - 1)); /* first_offset */
		file_writer_wb16(mov->fp, 0); /* reserved */
		file_writer_wb16(mov->fp, 1); /* reference_count */

		file_writer_wb32(mov->fp, 0); /* reference_type & referenced_size */
		file_writer_wb32(mov->fp, duration); /* subsegment_duration */
		file_writer_wb32(mov->fp, (1U/*starts_with_SAP*/ << 31) | (1 /*SAP_type*/ << 24) | 0 /*SAP_delta_time*/);
	}

	return 52 * mov->track_count;
}

static int fmp4_write_mfra(struct mov_t* mov)
{
	size_t i;
	uint64_t mfra_offset;
	uint64_t mfro_offset;

	// mfra
	mfra_offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "mfra", 4);

	// tfra
	for (i = 0; i < mov->track_count; i++)
	{
		mov->track = mov->tracks + i;
		mov_write_tfra(mov);
	}

	// mfro
	mfro_offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 16); /* size */
	file_writer_write(mov->fp, "mfro", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */
	file_writer_wb32(mov->fp, (uint32_t)(mfro_offset - mfra_offset + 16));

	mov_write_size(mov->fp, mfra_offset, (size_t)(mfro_offset - mfra_offset + 16));
	return (int)(mfro_offset - mfra_offset + 16);
}

static int fmp4_add_fragment_entry(struct mov_track_t* track, uint64_t time, uint64_t offset)
{
	if (track->frag_count >= track->frag_capacity)
	{
		void* p = realloc(track->frags, sizeof(struct mov_fragment_t) * (track->frag_capacity + 64));
		if (!p) return ENOMEM;
		track->frags = p;
		track->frag_capacity += 64;
	}

	track->frags[track->frag_count].time = time;
	track->frags[track->frag_count].offset = offset;
	++track->frag_count;
	return 0;
}

static int fmp4_writer_cluster(struct fmp4_writer_t* writer, struct mov_track_t* track, int flags)
{
	size_t i;
	struct mov_sample_t* cluster;
	cluster = track->samples + track->offset;

	assert(track->offset <= track->sample_count);
	if ((cluster->samples_per_chunk >= writer->frag_interleave && cluster->first_chunk + cluster->samples_per_chunk + 1 != writer->sn)
		|| (MOV_VIDEO == track->handler_type && (flags & MOV_AV_FLAG_KEYFREAME)))
	{
		// all track switch to new chunk(cluster)
		for (i = 0; i < writer->mov.track_count; i++)
			writer->mov.tracks[i].offset = writer->mov.tracks[i].sample_count;
		track->samples[track->offset].samples_per_chunk = 1;
		return 1;
	}
	else
	{
		cluster->samples_per_chunk += 1;
		return 0;
	}
}

static int fmp4_write_fragment(struct fmp4_writer_t* writer)
{
	size_t i, j, n;
	size_t refsize;
	uint64_t offset;
	uint64_t mdat_offset;
	struct mov_t* mov;
	struct mov_sample_t* cluster;
	mov = &writer->mov;

	assert(mov->fp);
	if (writer->mdat_size < 1 || !mov->fp)
		return 0; // empty

	// write moov
	if (!writer->has_moov && 0 == (mov->flags & MOV_FLAG_SEGMENT))
	{
		fmp4_write_moov(mov);
		writer->has_moov = 1;
	}

	if (mov->flags & MOV_FLAG_SEGMENT)
	{
		// ISO/IEC 23009-1:2014(E) 6.3.4.2 General format type (p93)
		// Each Media Segment may contain one or more ¡®sidx¡¯ boxes. 
		// If present, the first ¡®sidx¡¯ box shall be placed before any ¡®moof¡¯ box 
		// and the first Segment Index box shall document the entire Segment.
		fmp4_write_sidx(mov);
	}

	// moof
	mov->moof_offset = file_writer_tell(mov->fp);
	refsize = fmp4_write_moof(mov, ++writer->fragment_id); // start from 1
	refsize += writer->mdat_size + 8/*mdat box*/;

	// add mfra entry
	for (i = 0; i < mov->track_count; i++)
	{
		mov->track = mov->tracks + i;
		if (mov->track->sample_count > 0)
			fmp4_add_fragment_entry(mov->track, mov->track->samples[0].dts, mov->moof_offset);

		// hack: write sidx referenced_size
		if (mov->flags & MOV_FLAG_SEGMENT)
			mov_write_size(mov->fp, mov->moof_offset - 52 * (mov->track_count - i) + 40, (0 << 31) | (refsize & 0x7fffffff));

		mov->track->offset = 0; // reset
	}

	// mdat
	mdat_offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "mdat", 4);

	// interleave write cluster
	do
	{
		n = 0;
		for (i = 0; i < mov->track_count; i++)
		{
			mov->track = mov->tracks + i;
			assert(mov->track->offset <= mov->track->sample_count);
			if (mov->track->offset >= mov->track->sample_count)
				continue;

			cluster = mov->track->samples + mov->track->offset;
			assert(mov->track->offset + cluster->samples_per_chunk <= mov->track->sample_count);
			assert(cluster->samples_per_chunk > 0);

			// hack: write trun data offset
			mov_write_size(mov->fp, cluster->offset, (uint32_t)(file_writer_tell(mov->fp) - mov->moof_offset));

			for(j = (size_t)mov->track->offset; j < (size_t)mov->track->offset + cluster->samples_per_chunk; j++)
			{
				file_writer_write(mov->fp, mov->track->samples[j].data, mov->track->samples[j].bytes);
				free(mov->track->samples[j].data); // free av packet memory
			}

			mov->track->offset += cluster->samples_per_chunk;
			n++;
		}
	} while (n > 0);

	offset = file_writer_tell(mov->fp);
	mov_write_size(mov->fp, mdat_offset, (uint32_t)(offset - mdat_offset));

	for (i = 0; i < mov->track_count; i++)
	{
		mov->tracks[i].sample_count = 0; // clear track samples(don't free samples memory)
		mov->tracks[i].start_dts = INT64_MIN;
		mov->tracks[i].start_cts = INT64_MIN;
		mov->tracks[i].end_dts = 0;
		mov->tracks[i].offset = 0;
	}
	writer->mdat_size = 0;

	return file_writer_error(mov->fp);
}

static int fmp4_writer_init(struct mov_t* mov)
{
	if (mov->flags & MOV_FLAG_SEGMENT)
	{
		mov->ftyp.major_brand = MOV_BRAND_MSDH;
		mov->ftyp.minor_version = 0;
		mov->ftyp.brands_count = 4;
		mov->ftyp.compatible_brands[0] = MOV_BRAND_ISOM;
		mov->ftyp.compatible_brands[1] = MOV_BRAND_MP42;
		mov->ftyp.compatible_brands[2] = MOV_BRAND_MSDH;
		mov->ftyp.compatible_brands[3] = MOV_BRAND_MSIX;
		mov->header = 0;
	}
	else
	{
		mov->ftyp.major_brand = MOV_BRAND_ISOM;
		mov->ftyp.minor_version = 1;
		mov->ftyp.brands_count = 4;
		mov->ftyp.compatible_brands[0] = MOV_BRAND_ISOM;
		mov->ftyp.compatible_brands[1] = MOV_BRAND_MP42;
		mov->ftyp.compatible_brands[2] = MOV_BRAND_AVC1;
		mov->ftyp.compatible_brands[3] = MOV_BRAND_DASH;
		mov->header = 0;
	}
	return 0;
}

struct fmp4_writer_t* fmp4_writer_create(const char* file, int flags)
{
	struct mov_t* mov;
	struct fmp4_writer_t* writer;
	writer = (struct fmp4_writer_t*)calloc(1, sizeof(struct fmp4_writer_t));
	if (NULL == writer)
		return NULL;

	writer->frag_interleave = 5;

	mov = &writer->mov;
	mov->flags = flags;
	mov->mvhd.next_track_ID = 1;
	mov->mvhd.creation_time = time(NULL) + 0x7C25B080; // 1970 based -> 1904 based;
	mov->mvhd.modification_time = mov->mvhd.creation_time;
	mov->mvhd.timescale = 1000;
	mov->mvhd.duration = 0; // placeholder
	fmp4_writer_init(mov);

	if (0 != fmp4_writer_new_segment(writer, file))
	{
		free(writer);
		return NULL;
	}
	return writer;
}

void fmp4_writer_destroy(struct fmp4_writer_t* writer)
{
	size_t i, j;
	struct mov_t* mov;
	struct mov_track_t* track;
	mov = &writer->mov;

	fmp4_writer_save(writer);

	for (i = 0; i < mov->track_count; i++)
	{
		track = &mov->tracks[i];
		for (j = 0; j < track->sample_count; j++)
		{
			if (track->samples[j].data)
				free(track->samples[j].data);
		}
		if (track->extra_data) free(track->extra_data);
		if (track->samples) free(track->samples);
		if (track->frags) free(track->frags);
		if (track->stsd) free(track->stsd);
	}
	free(writer);
}

int fmp4_writer_write(struct fmp4_writer_t* writer, int idx, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	struct mov_track_t* track;
	struct mov_sample_t* sample;

	if (idx < 0 || idx >= (int)writer->mov.track_count)
		return -ENOENT;

	track = &writer->mov.tracks[idx];
	if (MOV_VIDEO == track->handler_type && (flags & MOV_AV_FLAG_KEYFREAME))
		fmp4_write_fragment(writer); // fragment per video keyframe

	if (track->sample_count + 1 >= track->sample_offset)
	{
		void* ptr = realloc(track->samples, sizeof(struct mov_sample_t) * (track->sample_offset + 1024));
		if (NULL == ptr) return -ENOMEM;
		track->samples = ptr;
		track->sample_offset += 1024;
	}

	pts = pts * track->mdhd.timescale / 1000;
	dts = dts * track->mdhd.timescale / 1000;

	if (INT64_MIN == track->start_dts)
		track->start_dts = dts;
	if (INT64_MIN == track->start_cts)
		track->start_cts = pts - dts;
	assert(track->end_dts <= dts);
	track->end_dts = dts;

	sample = &track->samples[track->sample_count];
	sample->sample_description_index = 1;
	sample->bytes = bytes;
	sample->flags = flags;
	sample->pts = pts;
	sample->dts = dts;
//	sample->offset = 0;

	sample->data = malloc(bytes);
	if (NULL == sample->data)
		return -ENOMEM;
	memcpy(sample->data, data, bytes);

	// cluster
	sample->samples_per_chunk = 0;
	sample->first_chunk = writer->sn++;
	fmp4_writer_cluster(writer, track, flags);

	writer->mdat_size += bytes; // update media data size
	track->sample_count += 1;
	return 0;
}

int fmp4_writer_add_audio(struct fmp4_writer_t* writer, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
{
	void* ptr = NULL;
	struct mov_t* mov;
	struct mov_stsd_t* stsd;
	struct mov_track_t* track;

	mov = &writer->mov;
	ptr = realloc(mov->tracks, sizeof(struct mov_track_t) * (mov->track_count + 1));
	if (NULL == ptr)
		return -ENOMEM;

	mov->tracks = ptr;
	track = &mov->tracks[mov->track_count];
	memset(track, 0, sizeof(struct mov_track_t));

	track->extra_data = malloc(extra_data_size + 1);
	if (NULL == track->extra_data)
		return -ENOMEM;
	memcpy(track->extra_data, extra_data, extra_data_size);
	track->extra_data_size = extra_data_size;

	track->stsd = calloc(1, sizeof(struct mov_stsd_t));
	if (NULL == track->stsd)
		return -ENOMEM;

	stsd = &track->stsd[0];
	stsd->data_reference_index = 1;
	stsd->object_type_indication = object;
	stsd->stream_type = MP4_STREAM_AUDIO;
	stsd->u.audio.channelcount = (uint16_t)channel_count;
	stsd->u.audio.samplesize = (uint16_t)bits_per_sample;
	stsd->u.audio.samplerate = (sample_rate > 56635 ? 0 : sample_rate) << 16;

	track->tag = mov_object_to_tag(object);
	track->handler_type = MOV_AUDIO;
	track->handler_descr = "SoundHandler";
	track->stsd_count = 1;
	track->start_dts = INT64_MIN;
	track->start_cts = INT64_MIN;
	track->end_dts = 0;
	track->offset = 0;

	track->tkhd.flags = MOV_TKHD_FLAG_TRACK_ENABLE | MOV_TKHD_FLAG_TRACK_IN_MOVIE;
	track->tkhd.track_ID = mov->mvhd.next_track_ID++;
	track->tkhd.creation_time = mov->mvhd.creation_time;
	track->tkhd.modification_time = mov->mvhd.modification_time;
	track->tkhd.width = 0;
	track->tkhd.height = 0;
	track->tkhd.volume = 0x0100;
	track->tkhd.duration = 0; // placeholder

	track->mdhd.creation_time = mov->mvhd.creation_time;
	track->mdhd.modification_time = mov->mvhd.modification_time;
	track->mdhd.timescale = sample_rate;
	track->mdhd.language = 0x55c4;
	track->mdhd.duration = 0; // placeholder

	return mov->track_count++;
}

int fmp4_writer_add_video(struct fmp4_writer_t* writer, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size)
{
	void* ptr = NULL;
	struct mov_t* mov;
	struct mov_stsd_t* stsd;
	struct mov_track_t* track;

	mov = &writer->mov;
	ptr = realloc(mov->tracks, sizeof(struct mov_track_t) * (mov->track_count + 1));
	if (NULL == ptr)
		return -ENOMEM;

	mov->tracks = ptr;
	track = &mov->tracks[mov->track_count];
	memset(track, 0, sizeof(struct mov_track_t));

	track->extra_data = malloc(extra_data_size + 1);
	if (NULL == track->extra_data)
		return -ENOMEM;
	memcpy(track->extra_data, extra_data, extra_data_size);
	track->extra_data_size = extra_data_size;

	track->stsd = calloc(1, sizeof(struct mov_stsd_t));
	if (NULL == track->stsd)
		return -ENOMEM;

	stsd = &track->stsd[0];
	stsd->data_reference_index = 1;
	stsd->object_type_indication = object;
	stsd->stream_type = MP4_STREAM_VISUAL;
	stsd->u.visual.width = (uint16_t)width;
	stsd->u.visual.height = (uint16_t)height;
	stsd->u.visual.depth = 0x0018;
	stsd->u.visual.frame_count = 1;
	stsd->u.visual.horizresolution = 0x00480000;
	stsd->u.visual.vertresolution = 0x00480000;

	track->tag = mov_object_to_tag(object);
	track->handler_type = MOV_VIDEO;
	track->handler_descr = "VideoHandler";
	track->stsd_count = 1;
	track->start_dts = INT64_MIN;
	track->start_cts = INT64_MIN;
	track->end_dts = 0;
	track->offset = 0;

	track->tkhd.flags = MOV_TKHD_FLAG_TRACK_ENABLE | MOV_TKHD_FLAG_TRACK_IN_MOVIE;
	track->tkhd.track_ID = mov->mvhd.next_track_ID++;
	track->tkhd.creation_time = mov->mvhd.creation_time;
	track->tkhd.modification_time = mov->mvhd.modification_time;
	track->tkhd.width = width << 16;
	track->tkhd.height = height << 16;
	track->tkhd.volume = 0;
	track->tkhd.duration = 0; // placeholder

	track->mdhd.creation_time = mov->mvhd.creation_time;
	track->mdhd.modification_time = mov->mvhd.modification_time;
	track->mdhd.timescale = 16000; //mov->mvhd.timescale
	track->mdhd.language = 0x55c4;
	track->mdhd.duration = 0; // placeholder

	return mov->track_count++;
}

static int fmp4_writer_save(fmp4_writer_t* writer)
{
	size_t i;
	struct mov_t* mov;
	mov = &writer->mov;

	if (!mov->fp)
		return -1;

	// flush fragment
	fmp4_write_fragment(writer);

	// write mfra
	if (0 == (mov->flags & MOV_FLAG_SEGMENT))
	{
		fmp4_write_mfra(mov);
		for (i = 0; i < mov->track_count; i++)
			mov->tracks[i].frag_count = 0; // don't free frags memory
	}

	file_writer_destroy(mov->fp);
	mov->fp = NULL;
	return 0;
}

int fmp4_writer_new_segment(fmp4_writer_t* writer, const char* file)
{
	struct mov_t* mov;
	mov = &writer->mov;

	fmp4_writer_save(writer);

	mov->fp = file_writer_create(file);
	if (!mov->fp)
		return -1;

	if (mov->flags & MOV_FLAG_SEGMENT)
	{
		mov_write_styp(mov);
	}
	else
	{
		mov_write_ftyp(mov);
	}

	return file_writer_error(mov->fp);
}

int fmp4_writer_init_segment(fmp4_writer_t* writer, const char* file)
{
	void* fp;
	struct mov_t* mov;
	mov = &writer->mov;
	fp = mov->fp;

	mov->fp = file_writer_create(file);
	if (!mov->fp)
	{
		mov->fp = fp;
		return -1;
	}

	mov_write_ftyp(mov);
	fmp4_write_moov(mov);
	file_writer_destroy(mov->fp);
	mov->fp = fp;
	return 0;
}
