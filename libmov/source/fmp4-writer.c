#include "fmp4-writer.h"
#include "mov-internal.h"
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

static size_t fmp4_write_mvex(struct mov_t* mov)
{
	size_t size, i;
	uint64_t offset;

	size = 8 /* Box */;
	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "mvex", 4);

	//size += fmp4_write_mehd(mov);
	for (i = 0; i < mov->track_count; i++)
	{
		mov->track = mov->tracks + i;
		size += mov_write_trex(mov);
	}
	//size += mov_write_leva(mov);

	mov_write_size(mov, offset, size); /* update size */
	return size;
}

static size_t fmp4_write_traf(struct mov_t* mov, uint32_t moof)
{
	size_t i, start, size;
	uint64_t offset;
    struct mov_track_t* track;

	size = 8 /* Box */;
	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "traf", 4);

    track = mov->track;
	track->tfhd.flags = MOV_TFHD_FLAG_DEFAULT_FLAGS /*| MOV_TFHD_FLAG_BASE_DATA_OFFSET*/;
    track->tfhd.flags |= MOV_TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX;
    // ISO/IEC 23009-1:2014(E) 6.3.4.2 General format type (p93)
	// The ¡®moof¡¯ boxes shall use movie-fragment relative addressing for media data that 
	// does not use external data references, the flag ¡®default-base-is-moof¡¯ shall be set, 
	// and data-offset shall be used, i.e. base-data-offset-present shall not be used.
	//if (mov->flags & MOV_FLAG_SEGMENT)
	{
		//track->tfhd.flags &= ~MOV_TFHD_FLAG_BASE_DATA_OFFSET;
		track->tfhd.flags |= MOV_TFHD_FLAG_DEFAULT_BASE_IS_MOOF;
	}
	track->tfhd.base_data_offset = mov->moof_offset;
    track->tfhd.sample_description_index = 1;
	track->tfhd.default_sample_flags = MOV_AUDIO == track->handler_type ? MOV_TREX_FLAG_SAMPLE_DEPENDS_ON_I_PICTURE : (MOV_TREX_FLAG_SAMPLE_IS_NO_SYNC_SAMPLE| MOV_TREX_FLAG_SAMPLE_DEPENDS_ON_NOT_I_PICTURE);
    if (track->sample_count > 0)
    {
        track->tfhd.flags |= MOV_TFHD_FLAG_DEFAULT_DURATION | MOV_TFHD_FLAG_DEFAULT_SIZE;
        track->tfhd.default_sample_duration = track->sample_count > 1 ? (uint32_t)(track->samples[1].dts - track->samples[0].dts) : 0;
        track->tfhd.default_sample_size = track->samples[0].bytes;
    }
    else
    {
        track->tfhd.flags |= MOV_TFHD_FLAG_DURATION_IS_EMPTY;
        track->tfhd.default_sample_duration = 0; // not set
        track->tfhd.default_sample_size = 0; // not set
    }

	size += mov_write_tfhd(mov);
	// ISO/IEC 23009-1:2014(E) 6.3.4.2 General format type (p93)
	// Each ¡®traf¡¯ box shall contain a ¡®tfdt¡¯ box.
    size += mov_write_tfdt(mov);

	for (start = 0, i = 1; i < track->sample_count; i++)
	{
        if (track->samples[i - 1].offset + track->samples[i - 1].bytes != track->samples[i].offset)
        {
            size += mov_write_trun(mov, start, i-start, moof);
            start = i;
        }
	}
    size += mov_write_trun(mov, start, i-start, moof);

	mov_write_size(mov, offset, size); /* update size */
	return size;
}

static size_t fmp4_write_moof(struct mov_t* mov, uint32_t fragment, uint32_t moof)
{
	size_t size, i;
	uint64_t offset;

	size = 8 /* Box */;
	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "moof", 4);

	size += mov_write_mfhd(mov, fragment);

	for (i = 0; i < mov->track_count; i++)
	{
		mov->track = mov->tracks + i;
		if (mov->track->sample_count > 0)
			size += fmp4_write_traf(mov, moof);
	}

	mov_write_size(mov, offset, size); /* update size */
	return size;
}

static size_t fmp4_write_moov(struct mov_t* mov)
{
	size_t size;
	size_t i, count;
	uint64_t offset;

	size = 8 /* Box */;
	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "moov", 4);

	size += mov_write_mvhd(mov);
//	size += fmp4_write_iods(mov);
	for (i = 0; i < mov->track_count; i++)
	{
		mov->track = mov->tracks + i;
		count = mov->track->sample_count;
		mov->track->sample_count = 0;
		size += mov_write_trak(mov);
		mov->track->sample_count = count; // restore sample count
	}

	size += fmp4_write_mvex(mov);
//  size += fmp4_write_udta(mov);
	mov_write_size(mov, offset, size); /* update size */
	return size;
}

static size_t fmp4_write_sidx(struct mov_t* mov)
{
	size_t i;
	for (i = 0; i < mov->track_count; i++)
	{
		mov->track = mov->tracks + i;
        mov_write_sidx(mov, 52 * (mov->track_count - i - 1)); /* first_offset */
	}

	return 52 * mov->track_count;
}

static int fmp4_write_mfra(struct mov_t* mov)
{
	size_t i;
	uint64_t mfra_offset;
	uint64_t mfro_offset;

	// mfra
	mfra_offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "mfra", 4);

	// tfra
	for (i = 0; i < mov->track_count; i++)
	{
		mov->track = mov->tracks + i;
		mov_write_tfra(mov);
	}

	// mfro
	mfro_offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 16); /* size */
	mov_buffer_write(&mov->io, "mfro", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	mov_buffer_w32(&mov->io, (uint32_t)(mfro_offset - mfra_offset + 16));

	mov_write_size(mov, mfra_offset, (size_t)(mfro_offset - mfra_offset + 16));
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

static int fmp4_write_fragment(struct fmp4_writer_t* writer)
{
	size_t i, n;
	size_t refsize;
	struct mov_t* mov;
	mov = &writer->mov;

	if (writer->mdat_size < 1)
		return 0; // empty

	// write moov
	if (!writer->has_moov)
	{
		// write ftyp/stype
		if (mov->flags & MOV_FLAG_SEGMENT)
		{
			mov_write_styp(mov);
		}
		else
		{
			mov_write_ftyp(mov);
			fmp4_write_moov(mov);
		}

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
	mov->moof_offset = mov_buffer_tell(&mov->io);
	refsize = fmp4_write_moof(mov, ++writer->fragment_id, 0); // start from 1
    // rewrite moof with trun data offset
    mov_buffer_seek(&mov->io, mov->moof_offset);
    fmp4_write_moof(mov, writer->fragment_id, refsize+8);
    refsize += writer->mdat_size + 8/*mdat box*/;

	// add mfra entry
	for (i = 0; i < mov->track_count; i++)
	{
		mov->track = mov->tracks + i;
		if (mov->track->sample_count > 0)
			fmp4_add_fragment_entry(mov->track, mov->track->samples[0].dts, mov->moof_offset);

		// hack: write sidx referenced_size
		if (mov->flags & MOV_FLAG_SEGMENT)
			mov_write_size(mov, mov->moof_offset - 52 * (mov->track_count - i) + 40, (0 << 31) | (refsize & 0x7fffffff));

		mov->track->offset = 0; // reset
	}

	// mdat
	mov_buffer_w32(&mov->io, writer->mdat_size + 8); /* size */
	mov_buffer_write(&mov->io, "mdat", 4);

	// interleave write samples
    n = 0;
	while(n < writer->mdat_size)
	{
		for (i = 0; i < mov->track_count; i++)
		{
			mov->track = mov->tracks + i;
			while (mov->track->offset < mov->track->sample_count && n == mov->track->samples[mov->track->offset].offset)
            {
                mov_buffer_write(&mov->io, mov->track->samples[mov->track->offset].data, mov->track->samples[mov->track->offset].bytes);
                free(mov->track->samples[mov->track->offset].data); // free av packet memory
                n += mov->track->samples[mov->track->offset].bytes;
                ++mov->track->offset;
            }
		}
	}

    // clear track samples(don't free samples memory)
	for (i = 0; i < mov->track_count; i++)
	{
		mov->tracks[i].sample_count = 0;
		mov->tracks[i].offset = 0;
	}
	writer->mdat_size = 0;

	return mov_buffer_error(&mov->io);
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

struct fmp4_writer_t* fmp4_writer_create(const struct mov_buffer_t *buffer, void* param, int flags)
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

	mov->io.param = param;
	memcpy(&mov->io.io, buffer, sizeof(mov->io.io));
	return writer;
}

void fmp4_writer_destroy(struct fmp4_writer_t* writer)
{
	size_t i;
	struct mov_t* mov;
	mov = &writer->mov;

	fmp4_writer_save_segment(writer);

	for (i = 0; i < mov->track_count; i++)
        mov_free_track(mov->tracks + i);
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

	sample = &track->samples[track->sample_count];
	sample->sample_description_index = 1;
	sample->bytes = bytes;
	sample->flags = flags;
	sample->pts = pts;
	sample->dts = dts;
	sample->offset = writer->mdat_size;

	sample->data = malloc(bytes);
	if (NULL == sample->data)
		return -ENOMEM;
	memcpy(sample->data, data, bytes);

    if (INT64_MIN == track->start_dts)
        track->start_dts = sample->dts;
	writer->mdat_size += bytes; // update media data size
	track->sample_count += 1;
	return 0;
}

int fmp4_writer_add_audio(struct fmp4_writer_t* writer, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
{
    struct mov_t* mov;
    struct mov_track_t* track;

    mov = &writer->mov;
    track = mov_add_track(mov);
    if (NULL == track)
        return -ENOMEM;

    if (0 != mov_add_audio(track, &mov->mvhd, 1000, object, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size))
        return -ENOMEM;

    mov->mvhd.next_track_ID++;
    return mov->track_count++;
}

int fmp4_writer_add_video(struct fmp4_writer_t* writer, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size)
{
    struct mov_t* mov;
    struct mov_track_t* track;

    mov = &writer->mov;
    track = mov_add_track(mov);
    if (NULL == track)
        return -ENOMEM;

    if (0 != mov_add_video(track, &mov->mvhd, 1000, object, width, height, extra_data, extra_data_size))
        return -ENOMEM;

    mov->mvhd.next_track_ID++;
    return mov->track_count++;
}

int fmp4_writer_add_subtitle(struct fmp4_writer_t* writer, uint8_t object, const void* extra_data, size_t extra_data_size)
{
    struct mov_t* mov;
    struct mov_track_t* track;

    mov = &writer->mov;
    track = mov_add_track(mov);
    if (NULL == track)
        return -ENOMEM;

    if (0 != mov_add_subtitle(track, &mov->mvhd, 1000, object, extra_data, extra_data_size))
        return -ENOMEM;

    mov->mvhd.next_track_ID++;
    return mov->track_count++;
}

int fmp4_writer_save_segment(fmp4_writer_t* writer)
{
	size_t i;
	struct mov_t* mov;
	mov = &writer->mov;

	// flush fragment
	fmp4_write_fragment(writer);
	writer->has_moov = 0; // clear moov flags

	// write mfra
	if (0 == (mov->flags & MOV_FLAG_SEGMENT))
	{
		fmp4_write_mfra(mov);
		for (i = 0; i < mov->track_count; i++)
			mov->tracks[i].frag_count = 0; // don't free frags memory
	}

	return mov_buffer_error(&mov->io);
}

int fmp4_writer_init_segment(fmp4_writer_t* writer)
{
	struct mov_t* mov;
	mov = &writer->mov;
	mov_write_ftyp(mov);
	fmp4_write_moov(mov);
	return mov_buffer_error(&mov->io);
}
