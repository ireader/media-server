#include "mkv-writer.h"
#include "mkv-format.h"
#include "mpeg4-aac.h"
#include "opus-head.h"
#include "mp3-header.h"
#include "flv-proto.h"
#include "flv-reader.h"
#include "flv-parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" const struct mkv_buffer_t* mkv_file_buffer(void);

static uint8_t s_buffer[2 * 1024 * 1024];
static int s_width, s_height;

static int onFLV(void* param, int codec, const void* data, size_t bytes, uint32_t pts, uint32_t dts, int flags)
{
	mkv_writer_t* mkv = (mkv_writer_t*)param;
	static int s_audio_track = -1;
	static int s_video_track = -1;

	switch(codec)
	{
	case FLV_AUDIO_AAC:
		return mkv_writer_write(mkv, s_audio_track, data, bytes, pts, dts, 0);
	
	case FLV_AUDIO_OPUS:
		return mkv_writer_write(mkv, s_audio_track, data, bytes, pts, dts, 0);

	case FLV_AUDIO_MP3:
		if (-1 == s_audio_track)
		{
			struct mp3_header_t mp3;
			if (0 == mp3_header_load(&mp3, data, bytes))
				return -1;
			s_audio_track = mkv_writer_add_audio(mkv, MKV_CODEC_AUDIO_MP3, mp3_get_channel(&mp3), 16, mp3_get_frequency(&mp3), NULL, 0);
		}

		if (-1 == s_audio_track)
			return -1;
		return mkv_writer_write(mkv, s_audio_track, data, bytes, pts, dts, 0);

	case FLV_VIDEO_H264:
	case FLV_VIDEO_H265:
	case FLV_VIDEO_AV1:
		return mkv_writer_write(mkv, s_video_track, data, bytes, pts, dts, 1 == flags ? MKV_FLAGS_KEYFRAME : 0);

	case FLV_VIDEO_AVCC:
		if (-1 == s_video_track)
			s_video_track = mkv_writer_add_video(mkv, MKV_CODEC_VIDEO_H264, s_width, s_height, data, bytes);
		break;

	case FLV_VIDEO_HVCC:
		if (-1 == s_video_track)
			s_video_track = mkv_writer_add_video(mkv, MKV_CODEC_VIDEO_H265, s_width, s_height, data, bytes);
		break;

	case FLV_VIDEO_AV1C:
		if (-1 == s_video_track)
			s_video_track = mkv_writer_add_video(mkv, MKV_CODEC_VIDEO_AV1, s_width, s_height, data, bytes);
		break;

	case FLV_AUDIO_ASC:
		if (-1 == s_audio_track)
		{
			struct mpeg4_aac_t aac;
			mpeg4_aac_audio_specific_config_load((const uint8_t*)data, bytes, &aac);
			int rate = mpeg4_aac_audio_frequency_to((enum mpeg4_aac_frequency)aac.sampling_frequency_index);
			s_audio_track = mkv_writer_add_audio(mkv, MKV_CODEC_AUDIO_AAC, aac.channel_configuration, 16, rate, data, bytes);
		}
		break;
	case FLV_AUDIO_OPUS_HEAD:
		if (-1 == s_audio_track)
		{
			struct opus_head_t opus;
			opus_head_load((const uint8_t*)data, bytes, &opus);
			s_audio_track = mkv_writer_add_audio(mkv, MKV_CODEC_AUDIO_OPUS, opus.channels, 16, opus.input_sample_rate, data, bytes);
		}
		break;

	case 0: // script
		break;

	default:
		// nothing to do
		assert(0);
	}

	printf("\n");
	return 0;
}

void mkv_writer_test(int w, int h, const char* inflv, const char* outmkv)
{
	int r, type;
	size_t taglen;
	uint32_t timestamp;

	struct mkv_file_cache_t wfile;
	memset(&wfile, 0, sizeof(wfile));
	wfile.fp = fopen(outmkv, "wb+");

	void* flv = flv_reader_create(inflv);
	mkv_writer_t* mkv = mkv_writer_create(mkv_file_cache_buffer(), &wfile, 0);

	s_width = w;
	s_height = h;
	while (1 == flv_reader_read(flv, &type, &timestamp, &taglen, s_buffer, sizeof(s_buffer)))
	{
		r = flv_parser_tag(type, s_buffer, taglen, timestamp, onFLV, mkv);
		assert(r >= 0);
	}

	mkv_writer_destroy(mkv);
	flv_reader_destroy(flv);
	fclose(wfile.fp);
}
