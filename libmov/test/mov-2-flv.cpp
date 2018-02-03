#include "mov-reader.h"
#include "mov-format.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "flv-proto.h"
#include "flv-writer.h"
#include "flv-muxer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static uint8_t s_packet[2 * 1024 * 1024];
static uint8_t s_buffer[4 * 1024 * 1024];
static uint32_t s_aac_track;
static uint32_t s_avc_track;
static uint32_t s_txt_track;
static uint8_t s_video_type;

static int iskeyframe(const uint8_t* data, size_t bytes)
{
	int nalu_length = 4;
	uint32_t len;

	while (bytes >= nalu_length + 1)
	{
		len = 0;
		for (int i = 0; i < nalu_length; i++)
			len = (len << 8) | data[i];

		if (len + nalu_length > bytes)
			return 0; // invalid

		uint8_t nalu_type = (FLV_VIDEO_H264 == s_video_type) ? (data[nalu_length] & 0x1f) : ((data[nalu_length] >> 1) & 0x3f);
		if ((FLV_VIDEO_H264 == s_video_type) ? (5 == nalu_type) : (16 <= nalu_type && nalu_type <= 23))
			return 1;

		bytes -= nalu_length + len;
		data += nalu_length + len;
	}

	return 0;
}

static void onread(void* flv, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts)
{
	if (s_avc_track == track)
	{
		int keyframe = iskeyframe((const uint8_t*)buffer, bytes);
		int compositionTime = (int)(pts - dts);
		printf("[V] pts: %08lld, dts: %08lld%s\n", pts, dts, keyframe ? " [I]" : "");
		s_packet[0] = ((keyframe ? 1 : 2) << 4) /* FrameType */ | s_video_type;
		s_packet[1] = 1; // AVC NALU
		s_packet[2] = (compositionTime >> 16) & 0xFF;
		s_packet[3] = (compositionTime >> 8) & 0xFF;
		s_packet[4] = compositionTime & 0xFF;
		memcpy(s_packet + 5, buffer, bytes);
		flv_writer_input(flv, 9, s_packet, bytes + 5, (uint32_t)dts);
	}
	else if (s_aac_track == track)
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

static void mov_video_info(void* flv, uint32_t track, uint8_t object, int /*width*/, int /*height*/, const void* extra, size_t bytes)
{
	s_avc_track = track;
	assert(MOV_OBJECT_H264 == object || MOV_OBJECT_HEVC == object);
	s_video_type = MOV_OBJECT_H264 == object ? FLV_VIDEO_H264 : FLV_VIDEO_H265;
	s_packet[0] = (1 << 4) /* FrameType */ | s_video_type;
	s_packet[1] = 0; // AVC sequence header
	s_packet[2] = 0; // CompositionTime 0
	s_packet[3] = 0;
	s_packet[4] = 0;
	memcpy(s_packet + 5, extra, bytes);
	flv_writer_input(flv, 9, s_packet, bytes + 5, 0);
}

static void mov_audio_info(void* flv, uint32_t track, uint8_t object, int channel_count, int /*bit_per_sample*/, int sample_rate, const void* extra, size_t bytes)
{
	s_aac_track = track;
	assert(MOV_OBJECT_AAC == object);
	s_packet[0] = (10 << 4) /* AAC */ | (3 << 2) /* SoundRate */ | (1 << 1) /* 16-bit samples */ | 1 /* Stereo sound */;
	s_packet[1] = 0; // AACPacketType: 0-AudioSpecificConfig(AAC sequence header)

#if 1
	struct mpeg4_aac_t aac;
	memset(&aac, 0, sizeof(aac));
	aac.profile = MPEG4_AAC_LC;
	aac.channel_configuration = channel_count;
	aac.sampling_frequency_index = mpeg4_aac_audio_frequency_from(sample_rate);

	mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &aac);
	int n = mpeg4_aac_audio_specific_config_save(&aac, s_packet + 2, sizeof(s_packet) - 2);
	flv_writer_input(flv, 8, s_packet, n + 2, 0);
#else
	memcpy(s_packet + 2, extra, bytes);
	flv_writer_input(flv, 8, s_packet, bytes + 2, 0);
#endif
}

static int mov_meta_info(void* flv, int type, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, FLV_TYPE_SCRIPT, data, bytes, 0);
}

void mov_2_flv_test(const char* mp4)
{
	snprintf((char*)s_packet, sizeof(s_packet), "%s.flv", mp4);

	FILE* fp = fopen(mp4, "rb");
	mov_reader_t* mov = mov_reader_create(mov_file_buffer(), fp);
	void* flv = flv_writer_create((char*)s_packet);

	//flv_muxer_t* muxer = flv_muxer_create(mov_meta_info, flv);
	//memset(&metadata, 0, sizeof(metadata));
	//metadata.videocodecid = FLV_VIDEO_H264;
	//metadata.videodatarate = 2000;
	//metadata.framerate = 25.0;
	//metadata.width = 1280;
	//metadata.height = 720;
	//flv_muxer_metadata(muxer, &metadata);
	//flv_muxer_destroy(muxer);

	struct mov_reader_trackinfo_t info = { mov_video_info, mov_audio_info };
	mov_reader_getinfo(mov, &info, flv);

	while (mov_reader_read(mov, s_buffer, sizeof(s_buffer), onread, flv) > 0)
	{
	}

	mov_reader_destroy(mov);
	flv_writer_destroy(flv);
	fclose(fp);
}
