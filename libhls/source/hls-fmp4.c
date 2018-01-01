#include "hls-fmp4.h"
#include "hls-param.h"
#include "mov-format.h"
#include "fmp4-writer.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define N_FILESIZE (100 * 1024 * 1024) // 100M

#define VMAX(a, b) ((a) > (b) ? (a) : (b))

struct hls_fmp4_t
{
	fmp4_writer_t* mp4;
	size_t maxsize; // max bytes per mp4 file

	int64_t duration;	// user setting segment duration
	int64_t dts_last;	// last packet dts
	int64_t dts;		// segment first dts
	int64_t pts;		// segment first pts

	int video_track;
	int audio_only_flag;// don't have video stream in segment

	struct hls_fmp4_handler_t handler;
	void* param;
};

struct hls_fmp4_t* hls_fmp4_create(int64_t duration, struct hls_fmp4_handler_t* handler, void* param)
{
	int flags;
	char file[256];
	struct hls_fmp4_t* hls;
	hls = (struct hls_fmp4_t*)malloc(sizeof(*hls));
	if (NULL == hls)
		return NULL;

	memset(hls, 0, sizeof(struct hls_fmp4_t));
	hls->maxsize = N_FILESIZE;
	hls->dts = hls->pts = PTS_NO_VALUE;
	hls->dts_last = PTS_NO_VALUE;
	hls->duration = duration;
	memcpy(&hls->handler, handler, sizeof(hls->handler));
	hls->param = param;
	hls->video_track = -1;

	if (0 != hls->handler.open(hls->param, file, sizeof(file)))
	{
		free(hls);
		return NULL;
	}

	flags = 0;
	//flags |= MOV_FLAG_FASTSTART;
	flags |= MOV_FLAG_SEGMENT;
	hls->mp4 = fmp4_writer_create(file, flags);
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
	char file[256];

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
			duration = ((force_new_segment || dts > hls->dts_last + 100) ? hls->dts_last : dts) - hls->dts;
			r = hls->handler.close(hls->param, hls->pts, hls->dts, duration);
			if (0 != r) return r;

			// new segment
			r = hls->handler.open(hls->param, file, sizeof(file));
			if (0 == r) r = fmp4_writer_new_segment(hls->mp4, file);
			if (0 != r) return r;
		}

		hls->pts = pts;
		hls->dts = dts;
		hls->audio_only_flag = 1;
	}

	if (NULL == data || 0 == bytes)
		return 0;

	if (hls->audio_only_flag && track == hls->video_track)
		hls->audio_only_flag = 0; // clear audio only flag

	hls->dts_last = dts;
	return fmp4_writer_write(hls->mp4, track, data, bytes, pts, dts, flags);
}

int hls_fmp4_init_segment(hls_fmp4_t* hls, const char* file)
{
	return fmp4_writer_init_segment(hls->mp4, file);
}