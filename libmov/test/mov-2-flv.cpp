#include "mov-reader.h"
#include "mov-format.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "flv-proto.h"
#include "flv-header.h"
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
static struct flv_audio_tag_header_t s_audio_tag;
static struct flv_video_tag_header_t s_video_tag;

static int iskeyframe(const uint8_t* data, size_t bytes)
{
	size_t nalu_length = 4;
	uint32_t len;

	while (bytes >= nalu_length + 1)
	{
		len = 0;
		for (size_t i = 0; i < nalu_length; i++)
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

static void onread(void* flv, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	if (s_avc_track == track)
	{
		int keyframe = (FLV_VIDEO_H264 == s_video_type || FLV_VIDEO_H265 == s_video_type) ? iskeyframe((const uint8_t*)buffer, bytes) : flags;
		printf("[V] pts: %08lld, dts: %08lld%s\n", pts, dts, keyframe ? " [I]" : "");
		s_video_tag.keyframe = (keyframe ? 1 : 2);
		s_video_tag.avpacket = FLV_AVPACKET;
		s_video_tag.cts = (int32_t)(pts - dts);
		flv_video_tag_header_write(&s_video_tag, s_packet, sizeof(s_packet));
		memcpy(s_packet + 5, buffer, bytes);
		flv_writer_input(flv, FLV_TYPE_VIDEO, s_packet, bytes + 5, (uint32_t)dts);
	}
	else if (s_aac_track == track)
	{
		printf("[A] pts: %08lld, dts: %08lld\n", pts, dts);
		s_audio_tag.avpacket = FLV_AVPACKET;
		int m = flv_audio_tag_header_write(&s_audio_tag, s_packet, sizeof(s_packet));
		memcpy(s_packet + m, buffer, bytes); // AAC exclude ADTS
		flv_writer_input(flv, FLV_TYPE_AUDIO, s_packet, bytes + m, (uint32_t)dts);
	}
	else
	{
		assert(0);
	}
}

static void mov_video_info(void* flv, uint32_t track, uint8_t object, int /*width*/, int /*height*/, const void* extra, size_t bytes)
{
	s_avc_track = track;
	assert(MOV_OBJECT_H264 == object || MOV_OBJECT_HEVC == object || MOV_OBJECT_AV1 == object || MOV_OBJECT_VP9 == object);
	s_video_type = MOV_OBJECT_H264 == object ? FLV_VIDEO_H264 : (MOV_OBJECT_HEVC == object ? FLV_VIDEO_H265 : FLV_VIDEO_AV1);
	s_video_tag.codecid = s_video_type;
	s_video_tag.keyframe = 1;
	s_video_tag.avpacket = FLV_SEQUENCE_HEADER;
	s_video_tag.cts = 0;
	flv_video_tag_header_write(&s_video_tag, s_packet, sizeof(s_packet));
	memcpy(s_packet + 5, extra, bytes);
	flv_writer_input(flv, FLV_TYPE_VIDEO, s_packet, bytes + 5, 0);
}

static void mov_audio_info(void* flv, uint32_t track, uint8_t object, int channel_count, int /*bit_per_sample*/, int sample_rate, const void* extra, size_t bytes)
{
	if (MOV_OBJECT_AAC == object || MOV_OBJECT_OPUS == object)
	{
		s_aac_track = track;
		s_audio_tag.codecid = MOV_OBJECT_AAC == object ? FLV_AUDIO_AAC : FLV_AUDIO_OPUS;
		s_audio_tag.rate = 3; // 44k-SoundRate
		s_audio_tag.bits = 1; // 16-bit samples
		s_audio_tag.channels = 1; // Stereo sound
		s_audio_tag.avpacket = FLV_SEQUENCE_HEADER;
		int m = flv_audio_tag_header_write(&s_audio_tag, s_packet, sizeof(s_packet));

		memcpy(s_packet + m, extra, bytes);
		flv_writer_input(flv, FLV_TYPE_AUDIO, s_packet, bytes + m, 0);
	}
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
