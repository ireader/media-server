#include "mov-reader.h"
#include "mov-format.h"
#include "../../libmov/test/mov-file-buffer.h"
#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "mpeg-ps.h"
#include "sys/system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <map>

static uint8_t s_buffer[2 * 1024 * 1024];
static uint8_t s_packet[2 * 1024 * 1024];
static std::map<int, int> s_objects;
static std::map<int, int> s_tracks;
static struct mpeg4_hevc_t s_hevc;
static struct mpeg4_avc_t s_avc;
static struct mpeg4_aac_t s_aac;

static void* ps_alloc(void* /*param*/, size_t bytes)
{
	static char s_buffer[2 * 1024 * 1024];
	assert(bytes <= sizeof(s_buffer));
	return s_buffer;
}

static void ps_free(void* /*param*/, void* /*packet*/)
{
	return;
}

static int ps_write(void* param, int stream, void* packet, size_t bytes)
{
	return 1 == fwrite(packet, bytes, 1, (FILE*)param) ? 0 : ferror((FILE*)param);
}

inline const char* ftimestamp(uint32_t t, char* buf)
{
	sprintf(buf, "%02u:%02u:%02u.%03u", t / 3600000, (t / 60000) % 60, (t / 1000) % 60, t % 1000);
	return buf;
}

static void mov_onread(void* ps, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	static char s_pts[64], s_dts[64];
	static int64_t v_pts, v_dts;
	static int64_t a_pts, a_dts;
	static int64_t x_pts, x_dts;

	auto it = s_tracks.find(track);
	if (it == s_tracks.end())
	{
		assert(0);
		return;
	}

	switch (s_objects.find(track)->second)
	{
	case MOV_OBJECT_H264:
	{
		printf("[H264] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u%s\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - v_pts), (int)(dts - v_dts), (unsigned int)bytes, flags ? " [I]" : "");
		v_pts = pts;
		v_dts = dts;

		assert(h264_is_new_access_unit((const uint8_t*)buffer + 4, bytes - 4));
		int n = h264_mp4toannexb(&s_avc, buffer, bytes, s_packet, sizeof(s_packet));
		ps_muxer_input((ps_muxer_t*)ps, it->second, flags ? 0x01 : 0x00, pts * 90, dts * 90, s_packet, bytes);
		break;
	}
	case MOV_OBJECT_HEVC:
	{
		uint8_t nalu_type = (((const uint8_t*)buffer)[4] >> 1) & 0x3F;
		uint8_t irap = 16 <= nalu_type && nalu_type <= 23;

		printf("[H265] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u%s,%d\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - v_pts), (int)(dts - v_dts), (unsigned int)bytes, flags ? " [I]" : "", (unsigned int)nalu_type);
		v_pts = pts;
		v_dts = dts;

		assert(h265_is_new_access_unit((const uint8_t*)buffer + 4, bytes - 4));
		int n = h265_mp4toannexb(&s_hevc, buffer, bytes, s_packet, sizeof(s_packet));
		ps_muxer_input((ps_muxer_t*)ps, it->second, flags ? 0x01 : 0x00, pts * 90, dts * 90, s_packet, bytes);
		break;
	}
	case MOV_OBJECT_AAC:
	{
		printf("[AAC] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - a_pts), (int)(dts - a_dts), (unsigned int)bytes);
		a_pts = pts;
		a_dts = dts;

		int n = mpeg4_aac_adts_save(&s_aac, bytes, s_packet, sizeof(s_packet));
		memcpy(s_packet+n, buffer, bytes);
		ps_muxer_input((ps_muxer_t*)ps, it->second, flags ? 0x01 : 0x00, pts * 90, dts * 90, s_packet, bytes+n);
		break;
	}
	default:
		printf("[X] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - x_pts), (int)(dts - x_dts), (unsigned int)bytes);
		x_pts = pts;
		x_dts = dts;
		break;
	}
}

static void mov_video_info(void* param, uint32_t track, uint8_t object, int width, int height, const void* extra, size_t bytes)
{
	s_objects[track] = object;
	if (MOV_OBJECT_H264 == object)
	{
		mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s_avc);
		s_tracks[track] = ps_muxer_add_stream((ps_muxer_t*)param, PSI_STREAM_H264, NULL, 0);
	}
	else if (MOV_OBJECT_HEVC == object)
	{
		mpeg4_hevc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s_hevc);
		s_tracks[track] = ps_muxer_add_stream((ps_muxer_t*)param, PSI_STREAM_H265, NULL, 0);
	}
}

static void mov_audio_info(void* param, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes)
{
	s_objects[track] = object;
	if (MOV_OBJECT_AAC == object)
	{
		assert(bytes == mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &s_aac));
		assert(channel_count == s_aac.channels);
		assert(MOV_OBJECT_AAC == object);
		s_aac.profile = MPEG4_AAC_LC;
		s_aac.channel_configuration = channel_count;
		s_aac.sampling_frequency_index = mpeg4_aac_audio_frequency_from(sample_rate);
		s_tracks[track]= ps_muxer_add_stream((ps_muxer_t*)param, PSI_STREAM_AAC, NULL, 0);
	}
}

void mov_2_mpeg_ps_test(const char* mp4)
{
	char output[256] = { 0 };
	snprintf(output, sizeof(output) - 1, "%s.ps", mp4);

	struct ps_muxer_func_t handler;
	handler.alloc = ps_alloc;
	handler.write = ps_write;
	handler.free = ps_free;

	FILE* fp = fopen(output, "wb");
	ps_muxer_t* ps = ps_muxer_create(&handler, fp);

	struct mov_file_cache_t file;
	memset(&file, 0, sizeof(file));
	file.fp = fopen(mp4, "rb");
	mov_reader_t* mov = mov_reader_create(mov_file_cache_buffer(), &file);

	struct mov_reader_trackinfo_t info = { mov_video_info, mov_audio_info };
	mov_reader_getinfo(mov, &info, ps);

	while (mov_reader_read(mov, s_buffer, sizeof(s_buffer), mov_onread, ps) > 0)
	{
	}

	mov_reader_destroy(mov);
	ps_muxer_destroy(ps);
	fclose(file.fp);
	fclose(fp);
}
