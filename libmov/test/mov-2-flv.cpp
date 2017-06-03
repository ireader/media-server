#include "mov-reader.h"
#include "mov-format.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "flv-writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define H264_NAL_IDR		5 // Coded slice of an IDR picture
#define H264_NAL_SPS		7 // Sequence parameter set
#define H264_NAL_PPS		8 // Picture parameter set

static uint8_t s_packet[2 * 1024 * 1024];
static uint8_t s_buffer[4 * 1024 * 1024];
static struct mpeg4_avc_t s_avc;

static void onread(void* flv, int avtype, const void* buffer, size_t bytes, int64_t pts, int64_t dts)
{
	if (MOV_AVC1 == avtype)
	{
		printf("[V] pts: %08lld, dts: %08lld\n", pts, dts);
		int compositionTime = (int)(pts - dts);
		assert(MOV_AVC1 == avtype);
		s_packet[0] = (1 << 4) /* FrameType */ | 7 /* AVC */;
		s_packet[1] = 1; // AVC NALU
		s_packet[2] = (compositionTime >> 16) & 0xFF;
		s_packet[3] = (compositionTime >> 8) & 0xFF;
		s_packet[4] = compositionTime & 0xFF;
		memcpy(s_packet + 5, buffer, bytes);
		flv_writer_input(flv, 9, s_packet, bytes + 5, (uint32_t)dts);
	}
	else if (MOV_MP4A == avtype)
	{
		printf("[A] pts: %08lld, dts: %08lld\n", pts, dts);
		s_packet[0] = (10 << 4) /* AAC */ | (3 << 2) /* 44k-SoundRate */ | (1 << 1) /* 16-bit samples */ | 1 /* Stereo sound */;
		s_packet[1] = 1; // AACPacketType: 1-AAC raw
		memcpy(s_packet + 2, buffer, bytes); // AAC exclude ADTS
		flv_writer_input(flv, 8, s_packet, bytes + 2, (uint32_t)dts);
	}
	else
	{
		assert(0);
	}
}

static void mov_video_info(void* flv, int avtype, int /*width*/, int /*height*/, const void* extra, size_t bytes)
{
	assert(MOV_AVC1 == avtype);
	s_packet[0] = (1 << 4) /* FrameType */ | 7 /* AVC */;
	s_packet[1] = 0; // AVC sequence header
	s_packet[2] = 0; // CompositionTime 0
	s_packet[3] = 0;
	s_packet[4] = 0;
	memcpy(s_packet + 5, extra, bytes);
	flv_writer_input(flv, 9, s_packet, bytes + 5, 0);
}

static void mov_audio_info(void* flv, int avtype, int /*channel_count*/, int /*bit_per_sample*/, int /*sample_rate*/, const void* extra, size_t bytes)
{
	assert(MOV_MP4A == avtype);
	s_packet[0] = (10 << 4) /* AAC */ | (3 << 2) /* SoundRate */ | (1 << 1) /* 16-bit samples */ | 1 /* Stereo sound */;
	s_packet[1] = 0; // AACPacketType: 0-AudioSpecificConfig(AAC sequence header)

#if 1
	struct mpeg4_aac_t aac;
	mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &aac);
	int n = mpeg4_aac_audio_specific_config_save(&aac, s_packet + 2, sizeof(s_packet) - 2);
	flv_writer_input(flv, 8, s_packet, n + 2, 0);
#else
	memcpy(s_packet + 2, extra, bytes);
	flv_writer_input(flv, 8, s_packet, bytes + 2, 0);
#endif
}

void mov_2_flv_test(const char* mp4)
{
	snprintf((char*)s_packet, sizeof(s_packet), "%s.flv", mp4);

	void* mov = mov_reader_create(mp4);
	void* flv = flv_writer_create((char*)s_packet);

	mov_reader_getinfo(mov, mov_video_info, mov_audio_info, flv);

	while (mov_reader_read(mov, s_buffer, sizeof(s_buffer), onread, flv) > 0)
	{
	}

	mov_reader_destroy(mov);
	flv_writer_destroy(flv);
}
