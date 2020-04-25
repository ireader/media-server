#include "mpeg-ps.h"
#include "hls-m3u8.h"
#include "hls-media.h"
#include "hls-param.h"
#include "flv-proto.h"
#include "flv-reader.h"
#include "flv-demuxer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int hls_handler(void* m3u8, const void* data, size_t bytes, int64_t pts, int64_t dts, int64_t duration)
{
	static int64_t s_dts = -1;
	int discontinue = -1 != s_dts ? 0 : (dts > s_dts + HLS_DURATION / 2 ? 1 : 0);
	s_dts = dts;

	static int i = 0;
	char name[128] = {0};
	snprintf(name, sizeof(name), "%d.ts", i++);
	hls_m3u8_add((hls_m3u8_t*)m3u8, name, pts, duration, discontinue);

	FILE* fp = fopen(name, "wb");
    if(fp)
    {
        fwrite(data, 1, bytes, fp);
        fclose(fp);
    }

	return 0;
}

static int flv_handler(void* param, int codec, const void* data, size_t bytes, uint32_t pts, uint32_t dts, int flags)
{
	hls_media_t* hls = (hls_media_t*)param;

	switch (codec)
	{
	case FLV_AUDIO_AAC:
		return hls_media_input(hls, STREAM_AUDIO_AAC, data, bytes, pts, dts, 0);

	case FLV_AUDIO_MP3:
		return hls_media_input(hls, STREAM_AUDIO_MP3, data, bytes, pts, dts, 0);

	case FLV_VIDEO_H264:
		return hls_media_input(hls, STREAM_VIDEO_H264, data, bytes, pts, dts, flags ? HLS_FLAGS_KEYFRAME : 0);

	case FLV_VIDEO_H265:
		return hls_media_input(hls, STREAM_VIDEO_H265, data, bytes, pts, dts, flags ? HLS_FLAGS_KEYFRAME : 0);

	default:
		// nothing to do
		return 0;
	}
}

void hls_segmenter_flv(const char* file)
{
	hls_m3u8_t* m3u = hls_m3u8_create(0, 3);
	hls_media_t* hls = hls_media_create(HLS_DURATION * 1000, hls_handler, m3u);
	void* flv = flv_reader_create(file);
	flv_demuxer_t* demuxer = flv_demuxer_create(flv_handler, hls);

	int r, type;
	uint32_t timestamp;
	static char data[2 * 1024 * 1024];
	while ((r = flv_reader_read(flv, &type, &timestamp, data, sizeof(data))) > 0)
	{
		flv_demuxer_input(demuxer, type, data, r, timestamp);
	}

	// write m3u8 file
	hls_media_input(hls, STREAM_VIDEO_H264, NULL, 0, 0, 0, 0);
	hls_m3u8_playlist(m3u, 1, data, sizeof(data));
	FILE* fp = fopen("playlist.m3u8", "wb");
    if(fp)
    {
        fwrite(data, 1, strlen(data), fp);
        fclose(fp);
    }

	flv_demuxer_destroy(demuxer);
	flv_reader_destroy(flv);
	hls_media_destroy(hls);
	hls_m3u8_destroy(m3u);
}
