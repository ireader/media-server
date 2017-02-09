#include "mov-writer.h"
#include "mov-internal.h"
#include "file-writer.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

static uint32_t mov_build_chunk(struct mov_track_t* track)
{
	size_t bytes, i;
	uint32_t count = 0;
	struct mov_sample_t* sample = NULL;

	for(i = 0; i < track->sample_count; i++)
	{
		if(i > 0 && sample->offset + bytes == track->samples[i].offset 
			&& sample->u.stsc.sample_description_index == track->samples[i].stsd->data_reference_index)
		{
			track->samples[i].u.stsc.first_chunk = 0; // mark invalid value
			bytes += track->samples[i].bytes;
			++sample->u.stsc.samples_per_chunk;
		}
		else
		{
			sample = &track->samples[i];
			sample->u.stsc.first_chunk = ++count;
			sample->u.stsc.samples_per_chunk = 1;
			sample->u.stsc.sample_description_index = sample->stsd->data_reference_index;
			bytes = sample->bytes;
		}
	}

	return count;
}

static int32_t mov_sample_duration(const struct mov_track_t* track, size_t idx)
{
	uint64_t next_dts;
	next_dts = idx + 1 < track->sample_count ? track->samples[idx + 1].dts : track->samples[idx].dts;
	return (int32_t)(next_dts - track->samples[idx].dts);
}

static uint32_t mov_build_stts(struct mov_track_t* track)
{
	size_t i;
	uint32_t duration, count = 0;
	struct mov_sample_t* sample;

	for (i = 0; i < track->sample_count; i++)
	{
		duration = mov_sample_duration(track, i);
		if (i > 0 && duration == sample->u.stts.duration)
		{
			track->samples[i].u.stts.count = 0;
			++sample->u.stts.count; // compress
		}
		else
		{
			sample = &track->samples[i];
			sample->u.stts.count = 1;
			sample->u.stts.duration = duration;
			++count;
		}
	}
	return count;
}

static uint32_t mov_build_ctts(struct mov_track_t* track)
{
	size_t i;
	uint32_t count = 0;
	struct mov_sample_t* sample;

	for (i = 0; i < track->sample_count; i++)
	{
		int32_t diff = (int32_t)(track->samples[i].pts - track->samples[i].dts);
		if (i > 0 && diff == sample->u.stts.duration)
		{
			track->samples[i].u.stts.count = 0;
			++sample->u.stts.count; // compress
		}
		else
		{
			sample = &track->samples[i];
			sample->u.stts.count = 1;
			sample->u.stts.duration = (int32_t)(sample->pts - sample->dts);
			++count;
		}
	}

	return count;
}

static int mov_write_edts()
{
}


// ISO/IEC 14496-12:2012(E) 6.2.3 Box Order (p23)
// It is recommended that the boxes within the Sample Table Box be in the following order: 
// Sample Description, Time to Sample, Sample to Chunk, Sample Size, Chunk Offset.
static int mov_write_stbl(const struct mov_t* mov)
{
	size_t size;
	uint32_t count;
	uint64_t offset;
	const struct mov_track_t* track = mov->track;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "stbl", 4);

	size += mov_write_stsd(mov);

	count = mov_build_stts(track);
	size += mov_write_stts(mov, count);
	count = mov_build_ctts(track);
	if (track->sample_count > 0 && (count > 1 || track->samples[0].u.stts.duration != 0))
		size += mov_write_ctts(mov, count);

	count = mov_build_chunk(track);
//	size += mov_write_stss(mov);
	size += mov_write_stsc(mov, count);
	size += mov_write_stsz(mov);
	size += mov_write_stco(mov, count);

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
