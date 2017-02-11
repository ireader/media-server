#include "mov-writer.h"
#include "mov-internal.h"
#include "file-writer.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

struct mov_writer_t
{
	struct mov_t mov;
	uint64_t mdat_size;
	uint64_t mdat_offset;
};

static uint32_t mov_build_chunk(struct mov_track_t* track)
{
	size_t bytes, i;
	uint32_t count = 0;
	struct mov_stsd_t* stsd = NULL;
	struct mov_sample_t* sample = NULL;

	assert(track->stsd_count > 0);
	stsd = &track->stsd[0];
	for(i = 0; i < track->sample_count; i++)
	{
		if(NULL != sample && sample->offset + bytes == track->samples[i].offset 
			&& sample->u.stsc.sample_description_index == stsd->data_reference_index)
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
			sample->u.stsc.sample_description_index = stsd->data_reference_index;
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
	struct mov_sample_t* sample = NULL;

	for (i = 0; i < track->sample_count; i++)
	{
		duration = mov_sample_duration(track, i);
		if (NULL != sample && duration == sample->u.stts.duration)
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
size_t mov_write_stbl(const struct mov_t* mov)
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
		if (mov->track->sample_count < 1)
			continue;
		size += mov_write_trak(mov);
	}
	
//  size += mov_write_mvex(mov);
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
	mov->ftyp.minor_version = 0x200;
	mov->ftyp.brands_count = 4;
	mov->ftyp.compatible_brands[0] = MOV_BRAND_ISOM;
	mov->ftyp.compatible_brands[1] = MOV_BRAND_ISO2;
	mov->ftyp.compatible_brands[2] = MOV_BRAND_AVC1;
	mov->ftyp.compatible_brands[3] = MOV_BRAND_MP41;
	mov->header = 0;
	return 0;
}

void* mov_writer_create(const char* file)
{
	struct mov_t* mov;
	struct mov_writer_t* writer;
	writer = (struct mov_writer_t*)malloc(sizeof(struct mov_writer_t));
	if (NULL == writer)
		return NULL;
	memset(writer, 0, sizeof(*writer));

	mov = &writer->mov;
	mov->fp = file_writer_create(file);
	if (NULL == mov->fp || 0 != mov_writer_init(mov))
	{
		mov_writer_destroy(writer);
		return NULL;
	}

	mov_write_ftyp(mov);

	// mdat
	writer->mdat_offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "mdat", 4);

	mov->mvhd.next_track_ID = 1;
	mov->mvhd.creation_time = time(NULL) + 0x7C25B080; // 1970 based -> 1904 based;
	mov->mvhd.modification_time = mov->mvhd.creation_time;
	mov->mvhd.timescale = 1000;
	mov->mvhd.duration = 0; // placeholder
	return writer;
}

void mov_writer_destroy(void* p)
{
	size_t i;
	struct mov_t* mov;
	struct mov_writer_t* writer;
	writer = (struct mov_writer_t*)p;
	mov = &writer->mov;

	// finish mdat box
	mov_write_size(mov->fp, writer->mdat_offset, writer->mdat_size+8); /* update size */

	// finish sample info
	for (i = 0; i < mov->track_count; i++)
	{
		mov->track = &mov->tracks[i];
		if(mov->track->sample_count < 1)
			continue;

		// pts in ms
		mov->track->tkhd.duration = mov->track->samples[mov->track->sample_count-1].pts * mov->mvhd.timescale / 1000;
		mov->track->mdhd.duration = mov->track->samples[mov->track->sample_count - 1].pts * mov->track->mdhd.timescale / 1000;
		if (mov->track->tkhd.duration > mov->mvhd.duration)
		{
			mov->mvhd.duration = mov->track->tkhd.duration;
		}
	}

	// write moov box
	mov_write_moov(mov);

	file_writer_destroy(&writer->mov.fp);
	free(writer);
}

static struct mov_track_t* mov_writer_find_track(struct mov_t* mov, uint32_t handler)
{
	size_t i;
	for (i = 0; i < mov->track_count; i++)
	{
		if (mov->tracks[i].handler_type == handler)
			return &mov->tracks[i];
	}
	return NULL;
}

static int mov_writer_write(void* p, uint32_t handler, const void* buffer, size_t bytes, int64_t pts, int64_t dts)
{
	struct mov_t* mov;
	struct mov_sample_t* sample;
	struct mov_writer_t* writer;
	writer = (struct mov_writer_t*)p;
	mov = &writer->mov;
	mov->track = mov_writer_find_track(mov, handler);
	if (NULL == mov->track)
		return ENOENT; // not found

	if (mov->track->sample_count + 1 >= mov->track->sample_offset)
	{
		void* ptr = realloc(mov->track->samples, sizeof(struct mov_sample_t) * (mov->track->sample_offset + 1024));
		if (NULL == ptr) return ENOMEM;
		mov->track->samples = ptr;
		mov->track->sample_offset += 1024;
	}

	sample = &mov->track->samples[mov->track->sample_count++];
	sample->offset = file_writer_tell(mov->fp);
	sample->bytes = bytes;
	sample->pts = pts;
	sample->dts = dts;
	sample->flags = 0;

	if (bytes != file_writer_write(mov->fp, buffer, bytes))
		return -1; // file write error

	writer->mdat_size += bytes; // update media data size
	return file_writer_error(mov->fp);
}

int mov_writer_write_audio(void* p, const void* buffer, size_t bytes, int64_t pts, int64_t dts)
{
	return mov_writer_write(p, MOV_AUDIO, buffer, bytes, pts, dts);
}

int mov_writer_write_video(void* p, const void* buffer, size_t bytes, int64_t pts, int64_t dts)
{
	return mov_writer_write(p, MOV_VIDEO, buffer, bytes, pts, dts);
}

int mov_writer_audio_meta(void* p, uint32_t avtype, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
{
	void* ptr = NULL;
	struct mov_t* mov;
	struct mov_stsd_t* stsd;
	mov = &((struct mov_writer_t*)p)->mov;

	ptr = realloc(mov->tracks, sizeof(struct mov_track_t) * (mov->track_count + 1));
	if (NULL == ptr)
		return ENOMEM;

	mov->tracks = ptr;
	mov->track = &mov->tracks[mov->track_count++];
	memset(mov->track, 0, sizeof(struct mov_track_t));

	mov->track->extra_data = malloc(extra_data_size);
	if (NULL == mov->track->extra_data)
		return ENOMEM;
	memcpy(mov->track->extra_data, extra_data, extra_data_size);
	mov->track->extra_data_size = extra_data_size;

	mov->track->stsd = malloc(sizeof(struct mov_stsd_t));
	if (NULL == mov->track->stsd)
		return ENOMEM;
	stsd = &mov->track->stsd[mov->track->stsd_count++];
	stsd->data_reference_index = 1;
	stsd->type = avtype;
	stsd->u.audio.channelcount = (uint16_t)channel_count;
	stsd->u.audio.samplesize = (uint16_t)bits_per_sample;
	stsd->u.audio.samplerate = sample_rate;

	mov->track->codec_id = avtype;
	mov->track->handler_type = MOV_AUDIO;

	mov->track->tkhd.flags = MOV_TKHD_FLAG_TRACK_ENABLE | MOV_TKHD_FLAG_TRACK_IN_MOVIE;
	mov->track->tkhd.track_ID = mov->mvhd.next_track_ID++;
	mov->track->tkhd.creation_time = mov->mvhd.creation_time;
	mov->track->tkhd.modification_time = mov->mvhd.modification_time;
	mov->track->tkhd.width = 0;
	mov->track->tkhd.height = 0;
	mov->track->tkhd.volume = 0x0100;
	mov->track->tkhd.duration = 0; // placeholder

	mov->track->mdhd.creation_time = mov->mvhd.creation_time;
	mov->track->mdhd.modification_time = mov->mvhd.modification_time;
	mov->track->mdhd.timescale = sample_rate;
	mov->track->mdhd.language = 0x55c4;
	mov->track->mdhd.duration = 0; // placeholder
	return 0;
}

int mov_writer_video_meta(void* p, uint32_t avtype, int width, int height, const void* extra_data, size_t extra_data_size)
{
	void* ptr = NULL;
	struct mov_t* mov;
	struct mov_stsd_t* stsd;
	mov = &((struct mov_writer_t*)p)->mov;

	ptr = realloc(mov->tracks, sizeof(struct mov_track_t) * (mov->track_count + 1));
	if (NULL == ptr)
		return ENOMEM;

	mov->tracks = ptr;
	mov->track = &mov->tracks[mov->track_count++];
	memset(mov->track, 0, sizeof(struct mov_track_t));

	mov->track->extra_data = malloc(extra_data_size);
	if (NULL == mov->track->extra_data)
		return ENOMEM;
	memcpy(mov->track->extra_data, extra_data, extra_data_size);
	mov->track->extra_data_size = extra_data_size;

	mov->track->stsd = malloc(sizeof(struct mov_stsd_t));
	if (NULL == mov->track->stsd)
		return ENOMEM;
	stsd = &mov->track->stsd[mov->track->stsd_count++];
	stsd->data_reference_index = 1;
	stsd->type = avtype;
	stsd->u.visual.width = (uint16_t)width;
	stsd->u.visual.height = (uint16_t)height;
	stsd->u.visual.depth = 0x0018;
	stsd->u.visual.frame_count = 1;
	stsd->u.visual.horizresolution = 0x00480000;
	stsd->u.visual.vertresolution = 0x00480000;

	mov->track->codec_id = avtype;
	mov->track->handler_type = MOV_VIDEO;
	
	mov->track->tkhd.flags = MOV_TKHD_FLAG_TRACK_ENABLE | MOV_TKHD_FLAG_TRACK_IN_MOVIE;
	mov->track->tkhd.track_ID = mov->mvhd.next_track_ID++;
	mov->track->tkhd.creation_time = mov->mvhd.creation_time;
	mov->track->tkhd.modification_time = mov->mvhd.modification_time;
	mov->track->tkhd.width = width << 16;
	mov->track->tkhd.height = height << 16;
	mov->track->tkhd.volume = 0;
	mov->track->tkhd.duration = 0; // placeholder

	mov->track->mdhd.creation_time = mov->mvhd.creation_time;
	mov->track->mdhd.modification_time = mov->mvhd.modification_time;
	mov->track->mdhd.timescale = mov->mvhd.timescale;
	mov->track->mdhd.language = 0x55c4;
	mov->track->mdhd.duration = 0; // placeholder
	return 0;
}
