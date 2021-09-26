#include "flv-parser.h"
#include "mov-writer.h"
#include "mov-format.h"
#include "mpeg4-aac.h"
#include "flv-proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static uint8_t s_buffer[2 * 1024 * 1024];
static int s_width, s_height;

static int OnAVPacket(void* param, int codec, const void* data, size_t bytes, uint32_t pts, uint32_t dts, int flags)
{
	mov_writer_t* mov = (mov_writer_t*)param;
	static int s_aac_track = -1;
	static int s_avc_track = -1;

	switch (codec)
	{
	case FLV_AUDIO_AAC:
		return mov_writer_write(mov, s_aac_track, data, bytes, pts, dts, 1 == flags ? MOV_AV_FLAG_KEYFREAME : 0);

	case FLV_AUDIO_MP3:
		assert(0);
		break;

	case FLV_VIDEO_H264:
		return mov_writer_write(mov, s_avc_track, data, bytes, pts, dts, flags);

	case FLV_VIDEO_AVCC:
		if (-1 == s_avc_track)
		{
			s_avc_track = mov_writer_add_video(mov, MOV_OBJECT_H264, s_width, s_height, data, bytes);
		}
		break;

	case FLV_AUDIO_ASC:
		if (-1 == s_aac_track)
		{
			struct mpeg4_aac_t aac;
			mpeg4_aac_audio_specific_config_load((const uint8_t*)data, bytes, &aac);
			int rate = mpeg4_aac_audio_frequency_to((enum mpeg4_aac_frequency)aac.sampling_frequency_index);
			s_aac_track = mov_writer_add_audio(mov, MOV_OBJECT_AAC, aac.channel_configuration, 16, rate, data, bytes);
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

void flv_parser_test(const char* flv)
{
	uint8_t buffer[1024];
	snprintf((char*)buffer, sizeof(buffer), "%s.mp4", flv);

	FILE* fp = fopen(flv, "rb");
	FILE* wfp = fopen((char*)buffer, "wb");
	mov_writer_t* mov = mov_writer_create(mov_file_buffer(), wfp, 0);

	flv_parser_t parser;
	memset(&parser, 0, sizeof(parser));

	for(int r = fread(buffer, 1, sizeof(buffer), fp); r > 0; r = fread(buffer, 1, sizeof(buffer), fp))
	{
		assert(0 == flv_parser_input(&parser, buffer, r, OnAVPacket, mov));
	}

	mov_writer_destroy(mov);
	fclose(wfp);
	fclose(fp);
}
