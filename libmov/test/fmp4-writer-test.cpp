#include "fmp4-writer.h"
#include "mov-format.h"
#include "mpeg4-aac.h"
#include "opus-head.h"
#include "mp3-header.h"
#include "flv-proto.h"
#include "flv-reader.h"
#include "flv-parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static uint8_t s_buffer[2 * 1024 * 1024];
static int s_width, s_height;

static int onFLV(void* param, int codec, const void* data, size_t bytes, uint32_t pts, uint32_t dts, int flags)
{
	fmp4_writer_t* mov = (fmp4_writer_t*)param;
	static int s_audio_track = -1;
	static int s_avc_track = -1;

	switch (codec)
	{
	case FLV_AUDIO_AAC:
	case FLV_AUDIO_OPUS:
		return fmp4_writer_write(mov, s_audio_track, data, bytes, pts, dts, 1 == flags ? MOV_AV_FLAG_KEYFREAME : 0);

	case FLV_AUDIO_MP3:
		if (-1 == s_audio_track)
		{
			struct mp3_header_t mp3;
			if (0 == mp3_header_load(&mp3, data, bytes))
				return -1;
			s_audio_track = fmp4_writer_add_audio(mov, MOV_OBJECT_MP3, mp3_get_channel(&mp3), 16, mp3_get_frequency(&mp3), NULL, 0);
		}

		if (-1 == s_audio_track)
			return -1;
		return fmp4_writer_write(mov, s_audio_track, data, bytes, pts, dts, 1 == flags ? MOV_AV_FLAG_KEYFREAME : 0);

	case FLV_AUDIO_G711A:
	case FLV_AUDIO_G711U:
		if (-1 == s_audio_track)
			s_audio_track = fmp4_writer_add_audio(mov, codec == FLV_AUDIO_G711A ? MOV_OBJECT_G711a : MOV_OBJECT_G711u, 1, 16, 8000, NULL, 0);
		if (-1 == s_audio_track)
			return -1;
		return fmp4_writer_write(mov, s_audio_track, data, bytes, pts, dts, 0);

	case FLV_VIDEO_H264:
		return fmp4_writer_write(mov, s_avc_track, data, bytes, pts, dts, flags);

	case FLV_VIDEO_AVCC:
		if (-1 == s_avc_track)
		{
			s_avc_track = fmp4_writer_add_video(mov, MOV_OBJECT_H264, s_width, s_height, data, bytes);
		}
		break;

	case FLV_AUDIO_ASC:
		if (-1 == s_audio_track)
		{
			struct mpeg4_aac_t aac;
			mpeg4_aac_audio_specific_config_load((const uint8_t*)data, bytes, &aac);
			int rate = mpeg4_aac_audio_frequency_to((enum mpeg4_aac_frequency)aac.sampling_frequency_index);
			s_audio_track = fmp4_writer_add_audio(mov, MOV_OBJECT_AAC, aac.channel_configuration, 16, rate, data, bytes);
		}
		break;

	case FLV_AUDIO_OPUS_HEAD:
		if (-1 == s_audio_track)
		{
			struct opus_head_t opus;
			opus_head_load((const uint8_t*)data, bytes, &opus);
			s_audio_track = fmp4_writer_add_audio(mov, MOV_OBJECT_OPUS, opus.channels, 16, opus.input_sample_rate, data, bytes);
		}
		break;

	default:
		// nothing to do
		//assert(0);
		break;
	}

	printf("\n");
	return 0;
}

void fmp4_writer_test(int w, int h, const char* inflv, const char* outmp4)
{
	int r, type;
	size_t taglen;
	uint32_t timestamp;
	FILE* fp = fopen(outmp4, "wb+");
	void* flv = flv_reader_create(inflv);
	fmp4_writer_t* mov = fmp4_writer_create(mov_file_buffer(), fp, 0);
	
	s_width = w;
	s_height = h;
	while (1 == flv_reader_read(flv, &type, &timestamp, &taglen, s_buffer, sizeof(s_buffer)))
	{
		r = flv_parser_tag(type, s_buffer, taglen, timestamp, onFLV, mov);
		assert(r >= 0);
	}

	fmp4_writer_destroy(mov);
	flv_reader_destroy(flv);
	fclose(fp);
}
