#include "hls-fmp4.h"
#include "hls-param.h"
#include "mov-format.h"
#include "fmp4-writer.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define VIDEO 0
#define AUDIO 1

#define N_TRACK 8
#define N_FILESIZE (100 * 1024 * 1024) // 100M

#define VMAX(a, b) ((a) > (b) ? (a) : (b))

struct hls_metadata_t
{
	int type;
	int track;
	uint8_t object;
	void* extra_data;
	size_t extra_data_size;

	// video
	int width;
	int height;

	// audio
	int channels;
	int bits_per_sample;
	int sample_rate;
};

struct hls_fmp4_t
{
	fmp4_writer_t* mp4;
	size_t maxsize; // max bytes per mp4 file

	int64_t duration;	// user setting segment duration
	int64_t dts_last;	// last packet dts
	int64_t dts;		// segment first dts
	int64_t pts;		// segment first pts

	int audio_only_flag;// don't have video stream in segment

	struct hls_fmp4_handler_t handler;
	void* param;

	struct hls_metadata_t tracks[N_TRACK];
	int track_count;
};

struct hls_fmp4_t* hls_fmp4_create(int64_t duration, struct hls_fmp4_handler_t* handler, void* param)
{
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
	return hls;
}

void hls_fmp4_destroy(struct hls_fmp4_t* hls)
{
	int i;

	if (hls->mp4)
	{
		fmp4_writer_destroy(hls->mp4);
		hls->mp4 = NULL;
	}

	for (i = 0; i < hls->track_count; i++)
	{
		if (hls->tracks[i].extra_data)
		{
			free(hls->tracks[i].extra_data);
			hls->tracks[i].extra_data = NULL;
		}
		hls->tracks[i].extra_data_size = 0;
	}

	free(hls);
}

int hls_fmp4_add_audio(hls_fmp4_t* hls, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
{
	hls->tracks[hls->track_count].type = AUDIO;
	hls->tracks[hls->track_count].object = object;
	hls->tracks[hls->track_count].channels = channel_count;
	hls->tracks[hls->track_count].bits_per_sample = bits_per_sample;
	hls->tracks[hls->track_count].sample_rate = sample_rate;
	hls->tracks[hls->track_count].extra_data = malloc(extra_data_size + 1);
	if (!hls->tracks[hls->track_count].extra_data)
		return -1; // no memory
	memcpy(hls->tracks[hls->track_count].extra_data, extra_data, extra_data_size);
	hls->tracks[hls->track_count].extra_data_size = extra_data_size;
	return hls->track_count++;
}

int hls_fmp4_add_video(hls_fmp4_t* hls, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size)
{
	hls->tracks[hls->track_count].type = VIDEO;
	hls->tracks[hls->track_count].object = object;
	hls->tracks[hls->track_count].width = width;
	hls->tracks[hls->track_count].height = height;
	hls->tracks[hls->track_count].extra_data = malloc(extra_data_size + 1);
	if (!hls->tracks[hls->track_count].extra_data)
		return -1; // no memory
	memcpy(hls->tracks[hls->track_count].extra_data, extra_data, extra_data_size);
	hls->tracks[hls->track_count].extra_data_size = extra_data_size;
	return hls->track_count++;
}

static int mp4_new_segment(struct hls_fmp4_t* hls)
{
	int i, r;
	int flags;
	char file[256];
	struct hls_metadata_t* t;

	r = hls->handler.open(hls->param, file, sizeof(file));
	if (0 != r)
		return r;

	flags = 0;
	//flags |= MOV_FLAG_FASTSTART;
	//flags |= MOV_FLAG_SEGMENT;
	hls->mp4 = fmp4_writer_create(file, flags);

	for (i = 0; i < hls->track_count; i++)
	{
		t = &hls->tracks[i];
		switch (t->type)
		{
		case AUDIO:
			t->track = fmp4_writer_add_audio(hls->mp4, t->object, t->channels, t->bits_per_sample, t->sample_rate, t->extra_data, t->extra_data_size);
			break;

		case VIDEO:
			t->track = fmp4_writer_add_video(hls->mp4, t->object, t->width, t->height, t->extra_data, t->extra_data_size);
			break;

		default:
			t->track = -1;
			assert(0);
			break;
		}

		if (t->track < 0)
		{
			fmp4_writer_destroy(hls->mp4);
			hls->mp4 = NULL;
			r = t->track;
			break;
		}
	}

	return r;
}

int hls_fmp4_input(struct hls_fmp4_t* hls, int track, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	int r, segment;
	int force_new_segment;
	int64_t duration;

	if (track >= hls->track_count)
		return -1; // invalid track value

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

	if (!hls->mp4 || segment || force_new_segment)
	{
		if (hls->mp4)
		{
			fmp4_writer_destroy(hls->mp4);
			duration = ((force_new_segment || dts > hls->dts_last + 100) ? hls->dts_last : dts) - hls->dts;
			r = hls->handler.close(hls->param, hls->pts, hls->dts, duration);
			if (0 != r) return r;
		}

		// new segment
		r = mp4_new_segment(hls);
		if (0 != r) return r;

		hls->pts = pts;
		hls->dts = dts;
		hls->audio_only_flag = 1;
	}

	if (hls->audio_only_flag && VIDEO == hls->tracks[track].type)
		hls->audio_only_flag = 0; // clear audio only flag

	hls->dts_last = dts;
	return fmp4_writer_write(hls->mp4, hls->tracks[track].track, data, bytes, pts, dts, flags);
}
