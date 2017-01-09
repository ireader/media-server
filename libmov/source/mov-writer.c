#include "mov-writer.h"
#include "mov-internal.h"
#include "file-writer.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

static void mov_build_chunk(struct mov_track_t* track)
{
	size_t bytes, i;
	struct mov_sample_t* sample = NULL;

	assert(0 == track->chunk_count);

	for(i = 0; i < track->stsz_count; i++)
	{
		if(i > 0 && sample->offset + bytes == track->samples[i].offset)
		{
			track->samples[i].u.chunk.first_chunk = 0; // mark invalid value
			bytes += track->samples[i].bytes;
			++sample->u.chunk.samples_per_chunk;
		}
		else
		{
			sample = &track->samples[i];
			sample->u.chunk.first_chunk = ++track->chunk_count;
			sample->u.chunk.samples_per_chunk = 1;
			sample->u.chunk.sample_description_index = track->id;
			bytes = sample->bytes;
		}
	}
}

static int32_t mov_sample_duration(const struct mov_track_t* track, size_t idx)
{
	uint64_t next_dts;
	next_dts = idx + 1 < track->stsz_count ? track->samples[idx + 1].dts : track->samples[idx].dts;
	return (int32_t)(next_dts - track->samples[idx].dts);
}

static void mov_build_stts(struct mov_track_t* track)
{
	size_t i;
	uint32_t duration;
	struct mov_sample_t* sample;

	track->chunk_count = 0;
	for (i = 0; i < track->stsz_count; i++)
	{
		duration = mov_sample_duration(track, i);
		if (i > 0 && duration == sample->u.timestamp.duration)
		{
			sample->u.timestamp.count = 0;
			++sample->u.timestamp.count; // compress
		}
		else
		{
			sample = &track->samples[i];
			sample->u.timestamp.count = 1;
			sample->u.timestamp.duration = duration;
			++track->chunk_count;
		}
	}
}

static void mov_build_ctts(struct mov_track_t* track)
{
	size_t i;
	struct mov_sample_t* sample;

	track->chunk_count = 0;
	for (i = 0; i < track->stsz_count; i++)
	{
		if (i > 0 && track->samples[i].pts - track->samples[i].dts == sample->u.timestamp.duration)
		{
			sample->u.timestamp.count = 0;
			++sample->u.timestamp.count; // compress
		}
		else
		{
			sample = &track->samples[i];
			sample->u.timestamp.count = 1;
			sample->u.timestamp.duration = (int32_t)(sample->pts - sample->dts);
			++track->chunk_count;
		}
	}
}

static int mov_write_edts()
{
}

static int mov_write_stbl(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;
	const struct mov_track_t* track = mov->track;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "stbl", 4);

	size += mov_write_stsd(mov);

	mov_build_stts(track);
	size += mov_write_stts(mov);
	if(track->stream_type == AVSTREAM_VIDEO)
	{
		mov_build_ctts(track);
		size += mov_write_ctts(mov);
	}

//	size += mov_write_stss(mov);
	size += mov_write_stsc(mov);
	size += mov_write_stsz(mov);
	size += mov_write_stco(mov);

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static size_t mov_write_dref(const struct mov_t* mov)
{
	file_writer_wb32(mov->fp, 28); /* size */
	file_writer_write(mov->fp, "dref", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */
	file_writer_wb32(mov->fp, 1); /* entry count */

	file_writer_wb32(mov->fp, 12); /* size */
	//FIXME add the alis and rsrc atom
	file_writer_write(mov->fp, "url ", 4);
	file_writer_wb32(mov->fp, 1); /* version & flags */

	return 28;
}

static size_t mov_write_dinf(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;
	
	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "dinf", 4);

	size += mov_write_dref(mov);

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static int mov_write_mdia(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;
	
	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "mdia", 4);

	size += mov_write_mdhd(mov);
	size += mov_write_hdlr(mov);
	size += mov_write_minf(mov);

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static size_t mov_write_trak(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "trak", 4);

	size += mov_write_tkhd(mov);
//	size += mov_write_tref(mov);
//	size += mov_write_edts(mov);
	size += mov_write_mdia(mov);

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static size_t mov_write_moov(struct mov_t* mov)
{
	size_t size, i;
	uint64_t offset;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "moov", 4);

	size += mov_write_mvhd(mov);
//	size += mov_write_iods(mov);
	for(i = 0; i < mov->track_count; i++)
	{
		mov->track = &mov->tracks[i];
		mov_build_chunk(&mov->tracks[i]);
		size += mov_write_trak(mov);
	}
	
//  size += mov_write_udta(mov);
	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

void mov_write_size(void* fp, uint64_t offset, size_t size)
{
	uint64_t offset2;
	offset2 = file_writer_tell(fp);
	file_writer_seek(fp, offset);
	file_writer_wb32(fp, size);
	file_writer_seek(fp, offset2);
}

static int mov_writer_init(struct mov_t* mov)
{
	mov->ftyp.major_brand = MOV_BRAND_ISOM;
	mov->ftyp.minor_version = 0;
	mov->ftyp.brands_count = 0;
	mov->header = 0;
	return 0;
}

void* mov_writer_create(const char* file)
{
	struct mov_t* mov;
	mov = (struct mov_t*)malloc(sizeof(*mov));
	if (NULL == mov)
		return NULL;

	memset(mov, 0, sizeof(*mov));
	mov->fp = file_writer_create(file);
	if (NULL == mov->fp || 0 != mov_writer_init(mov))
	{
		mov_writer_destroy(mov);
		return NULL;
	}

	return mov;
}

void mov_writer_destroy(void* p)
{
	struct mov_t* mov;
	mov = (struct mov_t*)p;
	file_writer_destroy(&mov->fp);
	free(mov);
}

int mov_writer_write(void* p, void* buffer, size_t bytes)
{
	struct mov_t* mov;
	mov = (struct mov_t*)p;
	return -1;
}
