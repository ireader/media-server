#include "mov-writer.h"
#include "mov-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

struct mov_writer_t
{
	struct mov_t mov;
	size_t mdat_size;
	uint64_t mdat_offset;
};

static uint32_t mov_build_chunk(struct mov_track_t* track)
{
	size_t i;
	size_t bytes = 0;
	uint32_t count = 0;
	struct mov_sample_t* sample = NULL;

	assert(track->stsd_count > 0);
	for(i = 0; i < track->sample_count; i++)
	{
		if(NULL != sample 
			&& sample->offset + bytes == track->samples[i].offset 
			&& sample->sample_description_index == track->samples[i].sample_description_index)
		{
			track->samples[i].first_chunk = 0; // mark invalid value
			bytes += track->samples[i].bytes;
			++sample->samples_per_chunk;
		}
		else
		{
			sample = &track->samples[i];
			sample->first_chunk = ++count; // chunk start from 1
			sample->samples_per_chunk = 1;
			bytes = sample->bytes;
		}
	}

	return count;
}

static uint32_t mov_build_stts(struct mov_track_t* track)
{
	size_t i;
	uint32_t delta, count = 0;
	struct mov_sample_t* sample = NULL;

	for (i = 0; i < track->sample_count; i++)
	{
		delta = (uint32_t)(i + 1 < track->sample_count ? track->samples[i + 1].dts - track->samples[i].dts : 0);
		if (NULL != sample && delta == sample->samples_per_chunk)
		{
			track->samples[i].first_chunk = 0;
			assert(sample->first_chunk > 0);
			++sample->first_chunk; // compress
		}
		else
		{
			sample = &track->samples[i];
			sample->first_chunk = 1;
			sample->samples_per_chunk = delta;
			++count;
		}
	}
	return count;
}

static uint32_t mov_build_ctts(struct mov_track_t* track)
{
	size_t i;
	int32_t delta;
	uint32_t count = 0;
	struct mov_sample_t* sample = NULL;

	for (i = 0; i < track->sample_count; i++)
	{
		delta = (int32_t)(track->samples[i].pts - track->samples[i].dts);
		if (i > 0 && delta == (int32_t)sample->samples_per_chunk)
		{
			track->samples[i].first_chunk = 0;
			assert(sample->first_chunk > 0);
			++sample->first_chunk; // compress
		}
		else
		{
			sample = &track->samples[i];
			sample->first_chunk = 1;
			sample->samples_per_chunk = delta;
			++count;
		}
	}

	return count;
}

static int mov_write_edts(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;

	size = 8 /* Box */;
	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "edts", 4);

	size += mov_write_elst(mov);

	mov_write_size(mov, offset, size); /* update size */
	return size;
}

// ISO/IEC 14496-12:2012(E) 6.2.3 Box Order (p23)
// It is recommended that the boxes within the Sample Table Box be in the following order: 
// Sample Description, Time to Sample, Sample to Chunk, Sample Size, Chunk Offset.
static size_t mov_write_stbl(const struct mov_t* mov)
{
	size_t size;
	uint32_t count;
	uint64_t offset;
	struct mov_track_t* track;
	track = (struct mov_track_t*)mov->track;

	size = 8 /* Box */;
	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "stbl", 4);

	size += mov_write_stsd(mov);

	count = mov_build_stts(track);
	size += mov_write_stts(mov, count);
	if(track->tkhd.width > 0 && track->tkhd.height > 0)
		size += mov_write_stss(mov); // video only
	count = mov_build_ctts(track);
	if (track->sample_count > 0 && (count > 1 || track->samples[0].samples_per_chunk != 0))
		size += mov_write_ctts(mov, count);

	count = mov_build_chunk(track);
	size += mov_write_stsc(mov);
	size += mov_write_stsz(mov);
	size += mov_write_stco(mov, count);

	mov_write_size(mov, offset, size); /* update size */
	return size;
}

static size_t mov_write_minf(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;
	const struct mov_track_t* track = mov->track;

	size = 8 /* Box */;
	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "minf", 4);

	if (MOV_VIDEO == track->handler_type)
	{
		size += mov_write_vmhd(mov);
	}
	else if (MOV_AUDIO == track->handler_type)
	{
		size += mov_write_smhd(mov);
	}
	else if (MOV_SUBT == track->handler_type)
	{
		size += mov_write_nmhd(mov);
	}
	else
	{
		assert(0);
	}

	size += mov_write_dinf(mov);
	size += mov_write_stbl(mov);
	mov_write_size(mov, offset, size); /* update size */
	return size;
}

static int mov_write_mdia(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;
	
	size = 8 /* Box */;
	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "mdia", 4);

	size += mov_write_mdhd(mov);
	size += mov_write_hdlr(mov);
	size += mov_write_minf(mov);

	mov_write_size(mov, offset, size); /* update size */
	return size;
}

static size_t mov_write_trak(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;

	size = 8 /* Box */;
	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "trak", 4);

	size += mov_write_tkhd(mov);
//	size += mov_write_tref(mov);
	size += mov_write_edts(mov);
	size += mov_write_mdia(mov);

	mov_write_size(mov, offset, size); /* update size */
	return size;
}

static size_t mov_write_moov(struct mov_t* mov)
{
	size_t size, i;
	uint64_t offset;

	size = 8 /* Box */;
	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "moov", 4);

	size += mov_write_mvhd(mov);
//	size += mov_write_iods(mov);
	for(i = 0; i < mov->track_count; i++)
	{
		mov->track = mov->tracks + i;
		if (mov->track->sample_count < 1)
			continue;
		size += mov_write_trak(mov);
	}

//  size += mov_write_udta(mov);
	mov_write_size(mov, offset, size); /* update size */
	return size;
}

void mov_write_size(const struct mov_t* mov, uint64_t offset, size_t size)
{
	uint64_t offset2;
	offset2 = mov_buffer_tell(&mov->io);
	mov_buffer_seek(&mov->io, offset);
	mov_buffer_w32(&mov->io, size);
	mov_buffer_seek(&mov->io, offset2);
}

static int mov_writer_init(struct mov_t* mov)
{
	mov->ftyp.major_brand = MOV_BRAND_ISOM;
	mov->ftyp.minor_version = 0x200;
	mov->ftyp.brands_count = 4;
	mov->ftyp.compatible_brands[0] = MOV_BRAND_ISOM;
	mov->ftyp.compatible_brands[1] = MOV_BRAND_ISO2;
	mov->ftyp.compatible_brands[2] = MOV_BRAND_AVC1;
	mov->ftyp.compatible_brands[3] = MOV_BRAND_MP41;
	mov->header = 0;
	return 0;
}

struct mov_writer_t* mov_writer_create(const struct mov_buffer_t* buffer, void* param, int flags)
{
	struct mov_t* mov;
	struct mov_writer_t* writer;
	writer = (struct mov_writer_t*)calloc(1, sizeof(struct mov_writer_t));
	if (NULL == writer)
		return NULL;

	mov = &writer->mov;
	mov->flags = flags;
	mov->io.param = param;
	memcpy(&mov->io.io, buffer, sizeof(mov->io.io));

	mov->mvhd.next_track_ID = 1;
	mov->mvhd.creation_time = time(NULL) + 0x7C25B080; // 1970 based -> 1904 based;
	mov->mvhd.modification_time = mov->mvhd.creation_time;
	mov->mvhd.timescale = 1000;
	mov->mvhd.duration = 0; // placeholder

	mov_writer_init(mov);
	mov_write_ftyp(mov);

	// mdat
	writer->mdat_offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "mdat", 4);
	return writer;
}

static int mov_writer_move(struct mov_t* mov, uint64_t to, uint64_t from, size_t bytes);
void mov_writer_destroy(struct mov_writer_t* writer)
{
	size_t i;
	uint64_t offset, offset2;
	struct mov_t* mov;
	struct mov_track_t* track;
	mov = &writer->mov;

	// finish mdat box
	mov_write_size(mov, writer->mdat_offset, writer->mdat_size+8); /* update size */

	// finish sample info
	for (i = 0; i < mov->track_count; i++)
	{
		track = &mov->tracks[i];
		if(track->sample_count < 1)
			continue;

		// pts in ms
		track->mdhd.duration = track->samples[track->sample_count - 1].dts - track->samples[0].dts;
		//track->mdhd.duration = track->mdhd.duration * track->mdhd.timescale / 1000;
		track->tkhd.duration = track->mdhd.duration * mov->mvhd.timescale / track->mdhd.timescale;
		if (track->tkhd.duration > mov->mvhd.duration)
			mov->mvhd.duration = track->tkhd.duration; // maximum track duration
	}

	// write moov box
	offset = mov_buffer_tell(&mov->io);
	mov_write_moov(mov);
	offset2 = mov_buffer_tell(&mov->io);
	
	if (MOV_FLAG_FASTSTART & mov->flags)
	{
		// check stco -> co64
		uint64_t co64 = 0;
		for (i = 0; i < mov->track_count; i++)
		{
			co64 += mov_stco_size(&mov->tracks[i], offset2 - offset);
		}

		if (co64)
		{
			uint64_t sz;
			do
			{
				sz = co64;
				co64 = 0;
				for (i = 0; i < mov->track_count; i++)
				{
					co64 += mov_stco_size(&mov->tracks[i], offset2 - offset + sz);
				}
			} while (sz != co64);
		}

		// rewrite moov
		for (i = 0; i < mov->track_count; i++)
			mov->tracks[i].offset += (offset2 - offset) + co64;

		mov_buffer_seek(&mov->io, offset);
		mov_write_moov(mov);
		assert(mov_buffer_tell(&mov->io) == offset2 + co64);
		offset2 = mov_buffer_tell(&mov->io);

		mov_writer_move(mov, writer->mdat_offset, offset, (size_t)(offset2 - offset));
	}

	for (i = 0; i < mov->track_count; i++)
	{
		track = &mov->tracks[i];
		if (track->extra_data) free(track->extra_data);
		if (track->samples) free(track->samples);
		if (track->stsd) free(track->stsd);
	}
	free(writer);
}

static int mov_writer_move(struct mov_t* mov, uint64_t to, uint64_t from, size_t bytes)
{
	uint8_t* ptr;
	uint64_t i, j;
	void* buffer[2];

	assert(bytes < INT32_MAX);
	ptr = malloc((size_t)(bytes * 2));
	if (NULL == ptr)
		return -ENOMEM;
	buffer[0] = ptr;
	buffer[1] = ptr + bytes;

	mov_buffer_seek(&mov->io, from);
	mov_buffer_read(&mov->io, buffer[0], bytes);

	j = 1;
	mov_buffer_seek(&mov->io, to);
	for (i = to; i < from; i += bytes)
	{
		mov_buffer_seek(&mov->io, i);
		mov_buffer_read(&mov->io, buffer[j], bytes);

		j ^= 1;
		mov_buffer_seek(&mov->io, i);
		mov_buffer_write(&mov->io, buffer[j], bytes);
	}

	mov_buffer_write(&mov->io, buffer[j], bytes - (size_t)(i - from));
	free(ptr);
	return mov_buffer_error(&mov->io);
}

int mov_writer_write(struct mov_writer_t* writer, int track, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	struct mov_t* mov;
	struct mov_sample_t* sample;

	if (track < 0 || track >= (int)writer->mov.track_count)
		return -ENOENT;
	
	mov = &writer->mov;
	mov->track = &mov->tracks[track];

	if (mov->track->sample_count + 1 >= mov->track->sample_offset)
	{
		void* ptr = realloc(mov->track->samples, sizeof(struct mov_sample_t) * (mov->track->sample_offset + 1024));
		if (NULL == ptr) return -ENOMEM;
		mov->track->samples = ptr;
		mov->track->sample_offset += 1024;
	}

	pts = pts * mov->track->mdhd.timescale / 1000;
	dts = dts * mov->track->mdhd.timescale / 1000;
	
	if (INT64_MIN == mov->track->start_dts)
		mov->track->start_dts = dts;
	if (INT64_MIN == mov->track->start_cts)
		mov->track->start_cts = pts - dts;
	assert(mov->track->end_dts <= dts);
	mov->track->end_dts = dts;

	sample = &mov->track->samples[mov->track->sample_count++];
	sample->sample_description_index = 1;
	sample->bytes = bytes;
	sample->flags = flags;
	sample->pts = pts;
	sample->dts = dts;
	
	sample->offset = mov_buffer_tell(&mov->io);
	mov_buffer_write(&mov->io, data, bytes);

	writer->mdat_size += bytes; // update media data size
	return mov_buffer_error(&mov->io);
}

int mov_writer_add_audio(struct mov_writer_t* writer, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
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

int mov_writer_add_video(struct mov_writer_t* writer, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size)
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

int mov_writer_add_subtitle(struct mov_writer_t* writer, uint8_t object, const void* extra_data, size_t extra_data_size)
{
	uint32_t tag;
	void* ptr = NULL;
	struct mov_t* mov;
	struct mov_stsd_t* stsd;
	struct mov_track_t* track;

	tag = mov_object_to_tag(object);
	if (0 == tag)
		return -ENOENT; // invalid object type

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
	stsd->stream_type = MP4_STREAM_VISUAL; // Visually composed tracks including video and text are layered using the ¡®layer¡¯ value.

	track->tag = tag;
	track->handler_type = MOV_SUBT;
	track->handler_descr = "SubtitleHandler";
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
	track->tkhd.volume = 0;
	track->tkhd.duration = 0; // placeholder

	track->mdhd.creation_time = mov->mvhd.creation_time;
	track->mdhd.modification_time = mov->mvhd.modification_time;
	track->mdhd.timescale = 1000; //mov->mvhd.timescale
	track->mdhd.language = 0x55c4;
	track->mdhd.duration = 0; // placeholder

	return mov->track_count++;
}
