#include "hls-fmp4.h"
#include "hls-param.h"
#include "mov-format.h"
#include "fmp4-writer.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define N_SEGMENT (1 * 1024 * 1024)
#define N_FILESIZE (100 * 1024 * 1024) // 100M

#define VMAX(a, b) ((a) > (b) ? (a) : (b))

struct hls_fmp4_t
{
	fmp4_writer_t* mp4;
	uint8_t* ptr;
	size_t bytes;
	size_t capacity;
	size_t offset;
	size_t maxsize; // max bytes per mp4 file

	int64_t duration;	// user setting segment duration
	int64_t dts_last;	// last packet dts
	int64_t dts;		// segment first dts
	int64_t pts;		// segment first pts

	int video_track;
	int audio_only_flag;// don't have video stream in segment

	hls_fmp4_handler handler;
	void* param;
};

static int mov_buffer_read(void* param, void* data, uint64_t bytes)
{
	struct hls_fmp4_t* fmp4;
	fmp4 = (struct hls_fmp4_t*)param;
	if (fmp4->offset + bytes > fmp4->bytes)
		return E2BIG;
	memcpy(data, fmp4->ptr + fmp4->offset, (size_t)bytes);
	return 0;
}

static int mov_buffer_write(void* param, const void* data, uint64_t bytes)
{
	void* ptr;
	size_t capacity;
	struct hls_fmp4_t* fmp4;
	fmp4 = (struct hls_fmp4_t*)param;
	if (fmp4->offset + bytes > fmp4->maxsize)
		return E2BIG;

	if (fmp4->offset + (size_t)bytes > fmp4->capacity)
	{
		capacity = fmp4->offset + (size_t)bytes + N_SEGMENT;
		capacity = capacity > fmp4->maxsize ? fmp4->maxsize : capacity;
		ptr = realloc(fmp4->ptr, capacity);
		if (NULL == ptr)
			return ENOMEM;
		fmp4->ptr = ptr;
		fmp4->capacity = capacity;
	}

	memcpy(fmp4->ptr + fmp4->offset, data, (size_t)bytes);
	fmp4->offset += (size_t)bytes;
	if(fmp4->offset > fmp4->bytes)
		fmp4->bytes = fmp4->offset;
	return 0;
}

static int mov_buffer_seek(void* param, uint64_t offset)
{
	struct hls_fmp4_t* fmp4;
	fmp4 = (struct hls_fmp4_t*)param;
	if (offset >= fmp4->maxsize)
		return E2BIG;
	fmp4->offset = (size_t)offset;
	return 0;
}

static uint64_t mov_buffer_tell(void* param)
{
	return ((struct hls_fmp4_t*)param)->offset;
}

static struct mov_buffer_t s_io = {
	mov_buffer_read,
	mov_buffer_write,
	mov_buffer_seek,
	mov_buffer_tell,
};

struct hls_fmp4_t* hls_fmp4_create(int64_t duration, hls_fmp4_handler handler, void* param)
{
	int flags;
	struct hls_fmp4_t* hls;
	hls = (struct hls_fmp4_t*)calloc(1, sizeof(*hls));
	if (NULL == hls)
		return NULL;

	hls->video_track = -1;
	hls->maxsize = N_FILESIZE;
	hls->dts = hls->pts = PTS_NO_VALUE;
	hls->dts_last = PTS_NO_VALUE;
	hls->duration = duration;
	hls->handler = handler;
	hls->param = param;

	flags = 0;
	//flags |= MOV_FLAG_FASTSTART;
	flags |= MOV_FLAG_SEGMENT;
	hls->mp4 = fmp4_writer_create(&s_io, hls, flags);
	if (NULL == hls->mp4)
	{
		free(hls);
		return NULL;
	}

	return hls;
}

void hls_fmp4_destroy(struct hls_fmp4_t* hls)
{
	if (hls->mp4)
	{
		fmp4_writer_destroy(hls->mp4);
		hls->mp4 = NULL;
	}

	if (hls->ptr)
	{
		free(hls->ptr);
		hls->ptr = NULL;
	}

	free(hls);
}

int hls_fmp4_add_audio(hls_fmp4_t* hls, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
{
	return fmp4_writer_add_audio(hls->mp4, object, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size);
}

int hls_fmp4_add_video(hls_fmp4_t* hls, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size)
{
	hls->video_track = fmp4_writer_add_video(hls->mp4, object, width, height, extra_data, extra_data_size);
	return hls->video_track;
}

int hls_fmp4_input(struct hls_fmp4_t* hls, int track, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	int r, segment;
	int force_new_segment;
	int64_t duration;

	assert(dts < hls->dts_last + hls->duration || PTS_NO_VALUE == hls->dts_last);

	// PTS/DTS rewind
	force_new_segment = 0;
	if (dts + hls->duration < hls->dts_last || NULL == data || 0 == bytes)
		force_new_segment = 1;

	if ((MOV_AV_FLAG_KEYFREAME & flags) && (dts - hls->dts >= hls->duration || 0 == hls->duration))
	{
		segment = 1;
	}
	else if (hls->audio_only_flag && dts - hls->dts >= hls->duration)
	{
		// audio only file
		segment = 1;
	}
	else
	{
		segment = 0;
	}

	if (PTS_NO_VALUE == hls->dts_last || segment || force_new_segment)
	{
		if (PTS_NO_VALUE != hls->dts_last)
		{
			// save and create new segment
			r = fmp4_writer_save_segment(hls->mp4);
			if (0 == r)
			{
				duration = ((force_new_segment || dts > hls->dts_last + 100) ? hls->dts_last : dts) - hls->dts;
				r = hls->handler(hls->param, hls->ptr, hls->bytes, hls->pts, hls->dts, duration);
			}
			if (0 != r) return r;
		}

		hls->pts = pts;
		hls->dts = dts;
		hls->audio_only_flag = 1;
		hls->offset = 0;
		hls->bytes = 0;
	}

	if (NULL == data || 0 == bytes)
		return 0;

	if (hls->audio_only_flag && track == hls->video_track)
		hls->audio_only_flag = 0; // clear audio only flag

	hls->dts_last = dts;
	return fmp4_writer_write(hls->mp4, track, data, bytes, pts, dts, flags);
}

int hls_fmp4_init_segment(hls_fmp4_t* hls, void* data, size_t bytes)
{
	int r;
	uint8_t* ptr;
	size_t len;
	size_t capacity;
	size_t offset;
	size_t maxsize;
	
	// save
	ptr = hls->ptr;
	len = hls->bytes;
	offset = hls->offset;
	capacity = hls->capacity;
	maxsize = hls->maxsize;

	hls->ptr = (uint8_t*)data;
	hls->bytes = 0;
	hls->offset = 0;
	hls->capacity = bytes;
	hls->maxsize = bytes;

	r = fmp4_writer_init_segment(hls->mp4);
	r = 0 ==  r ? (int)hls->bytes : -1;

	// restore
	hls->ptr = ptr;
	hls->bytes = len;
	hls->offset = offset;
	hls->capacity = capacity;
	hls->maxsize = maxsize;
	return r;
}
