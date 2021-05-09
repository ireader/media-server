#include "mov-reader.h"
#include "mov-format.h"
#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "opus-head.h"
#include "webm-vpx.h"
#include "aom-av1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "mov-file-buffer.h"

#define USE_NEW_MOV_READ_API 1

static uint8_t s_packet[2 * 1024 * 1024];
static uint8_t s_buffer[4 * 1024 * 1024];
static FILE *s_vfp, *s_afp;
static struct mpeg4_hevc_t s_hevc;
static struct mpeg4_avc_t s_avc;
static struct mpeg4_aac_t s_aac;
static struct webm_vpx_t s_vpx;
static struct opus_head_t s_opus;
static struct aom_av1_t s_av1;
static uint32_t s_aac_track = 0xFFFFFFFF;
static uint32_t s_avc_track = 0xFFFFFFFF;
static uint32_t s_av1_track = 0xFFFFFFFF;
static uint32_t s_vpx_track = 0xFFFFFFFF;
static uint32_t s_hevc_track = 0xFFFFFFFF;
static uint32_t s_opus_track = 0xFFFFFFFF;
static uint32_t s_mp3_track = 0xFFFFFFFF;
static uint32_t s_subtitle_track = 0xFFFFFFFF;

#if defined(USE_NEW_MOV_READ_API)
struct mov_packet_test_t
{
	int flags;
	int64_t pts;
	int64_t dts;
	uint32_t track;

	void* ptr;
	size_t bytes;
};
static void* onalloc(void* param, uint32_t track, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	// emulate allocation
	struct mov_packet_test_t* pkt = (struct mov_packet_test_t*)param;
	if (pkt->bytes < bytes)
		return NULL;
	pkt->flags = flags;
	pkt->pts = pts;
	pkt->dts = dts;
	pkt->track = track;
	pkt->bytes = bytes;
	return pkt->ptr;
}
#endif

inline const char* ftimestamp(uint32_t t, char* buf)
{
	sprintf(buf, "%02u:%02u:%02u.%03u", t / 3600000, (t / 60000) % 60, (t / 1000) % 60, t % 1000);
	return buf;
}

static void onread(void* flv, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	static char s_pts[64], s_dts[64];
	static int64_t v_pts, v_dts;
	static int64_t a_pts, a_dts;

	if (s_avc_track == track)
	{
		printf("[H264] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u%s\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - v_pts), (int)(dts - v_dts), (unsigned int)bytes, flags ? " [I]" : "");
		v_pts = pts;
		v_dts = dts;

		int n = h264_mp4toannexb(&s_avc, buffer, bytes, s_packet, sizeof(s_packet));
		fwrite(s_packet, 1, n, s_vfp);
	}
	else if (s_hevc_track == track)
	{
		uint8_t nalu_type = (((const uint8_t*)buffer)[4] >> 1) & 0x3F;
		uint8_t irap = 16 <= nalu_type && nalu_type <= 23;

		printf("[H265] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u%s,%d\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - v_pts), (int)(dts - v_dts), (unsigned int)bytes, flags ? " [I]" : "", (unsigned int)nalu_type);
		v_pts = pts;
		v_dts = dts;

		assert(h265_is_new_access_unit((const uint8_t*)buffer+4, bytes-4));
		int n = h265_mp4toannexb(&s_hevc, buffer, bytes, s_packet, sizeof(s_packet));
		fwrite(s_packet, 1, n, s_vfp);
	}
	else if (s_av1_track == track)
	{
		printf("[AV1] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u%s\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - v_pts), (int)(dts - v_dts), (unsigned int)bytes, flags ? " [I]" : "");
		v_pts = pts;
		v_dts = dts;

		//int n = aom_av1_codec_configuration_record_save(&s_av1, s_packet, sizeof(s_packet));
		//fwrite(s_packet, 1, n, s_vfp);
	}
    else if (s_vpx_track == track)
    {
        printf("[VP9] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u%s\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - v_pts), (int)(dts - v_dts), (unsigned int)bytes, flags ? " [I]" : "");
        v_pts = pts;
        v_dts = dts;

        //int n = aom_av1_codec_configuration_record_save(&s_av1, s_packet, sizeof(s_packet));
        //fwrite(s_packet, 1, n, s_vfp);
    }
	else if (s_aac_track == track)
	{
		printf("[AAC] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - a_pts), (int)(dts - a_dts), (unsigned int)bytes);
		a_pts = pts;
		a_dts = dts;

		uint8_t adts[32];
		int n = mpeg4_aac_adts_save(&s_aac, bytes, adts, sizeof(adts));
		fwrite(adts, 1, n, s_afp);
		fwrite(buffer, 1, bytes, s_afp);
	}
	else if (s_opus_track == track)
	{
		printf("[OPUS] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - a_pts), (int)(dts - a_dts), (unsigned int)bytes);
		a_pts = pts;
		a_dts = dts;
	}
	else if (s_mp3_track == track)
	{
		printf("[MP3] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - a_pts), (int)(dts - a_dts), (unsigned int)bytes);
		a_pts = pts;
		a_dts = dts;
		fwrite(buffer, 1, bytes, s_afp);
	}
	else if (s_subtitle_track == track)
	{
		static int64_t t_pts, t_dts;
		printf("[TEXT] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u, text: %.*s\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - t_pts), (int)(dts - t_dts), (unsigned int)bytes, (int)bytes-2, (const char*)buffer+2);
		t_pts = pts;
		t_dts = dts;
	}
	else
	{
		printf("%d\n", track);
		//assert(0);
	}
}

static void mov_video_info(void* /*param*/, uint32_t track, uint8_t object, int /*width*/, int /*height*/, const void* extra, size_t bytes)
{
	if (MOV_OBJECT_H264 == object)
	{
		s_vfp = fopen("v.h264", "wb");
		s_avc_track = track;
		mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s_avc);
	}
	else if (MOV_OBJECT_HEVC == object)
	{
		s_vfp = fopen("v.h265", "wb");
		s_hevc_track = track;
		mpeg4_hevc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s_hevc);
	}
	else if (MOV_OBJECT_AV1 == object)
	{
		s_vfp = fopen("v.obus", "wb");
		s_av1_track = track;
		aom_av1_codec_configuration_record_load((const uint8_t*)extra, bytes, &s_av1);
	}
    else if (MOV_OBJECT_VP9 == object)
    {
        s_vfp = fopen("v.vp9", "wb");
        s_vpx_track = track;
        webm_vpx_codec_configuration_record_load((const uint8_t*)extra, bytes, &s_vpx);
    }
	else
	{
		assert(0);
	}
}

static void mov_audio_info(void* /*param*/, uint32_t track, uint8_t object, int channel_count, int /*bit_per_sample*/, int sample_rate, const void* extra, size_t bytes)
{
	if (MOV_OBJECT_AAC == object)
	{
		s_afp = fopen("a.aac", "wb");
		s_aac_track = track;
		assert(bytes == mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &s_aac));
		assert(channel_count == s_aac.channels);
		assert(MOV_OBJECT_AAC == object);
		s_aac.profile = MPEG4_AAC_LC;
		s_aac.channel_configuration = channel_count;
		s_aac.sampling_frequency_index = mpeg4_aac_audio_frequency_from(sample_rate);
	}
	else if (MOV_OBJECT_OPUS == object)
	{
		s_afp = fopen("a.opus", "wb");
		s_opus_track = track;
		assert(bytes == opus_head_load((const uint8_t*)extra, bytes, &s_opus));
		assert(s_opus.input_sample_rate == 48000);
	}
	else if (MOV_OBJECT_MP3 == object || MOV_OBJECT_MP1A == object)
	{
		s_afp = fopen("a.mp3", "wb");
		s_mp3_track = track;
	}
	else
	{
		s_aac_track = track;
		s_aac.channel_configuration = channel_count;
		//s_aac.sampling_frequency_index = mpeg4_aac_audio_frequency_from(sample_rate);
	}
}

static void mov_subtitle_info(void* /*param*/, uint32_t track, uint8_t object, const void* /*extra*/, size_t /*bytes*/)
{
	s_subtitle_track = track;
}

void mov_reader_test(const char* mp4)
{
	struct mov_file_cache_t file;
	memset(&file, 0, sizeof(file));
	file.fp = fopen(mp4, "rb");
	mov_reader_t* mov = mov_reader_create(mov_file_cache_buffer(), &file);
	uint64_t duration = mov_reader_getduration(mov);

	struct mov_reader_trackinfo_t info = { mov_video_info, mov_audio_info, mov_subtitle_info };
	mov_reader_getinfo(mov, &info, NULL);

#if !defined(USE_NEW_MOV_READ_API)
	while (mov_reader_read(mov, s_buffer, sizeof(s_buffer), onread, NULL) > 0)
	{
	}
#else
	while (1)
	{
		struct mov_packet_test_t pkt;
		pkt.ptr = s_buffer;
		pkt.bytes = sizeof(s_buffer);
		int r = mov_reader_read2(mov, onalloc, &pkt);
		if (r <= 0)
		{
			// WARNNING: free(pkt.ptr) if alloc new buffer
			break;
		}
		onread(NULL, pkt.track, pkt.ptr, pkt.bytes, pkt.pts, pkt.dts, pkt.flags);
	}
#endif

	duration /= 2;
	mov_reader_seek(mov, (int64_t*)&duration);

	mov_reader_destroy(mov);
	if(s_vfp) fclose(s_vfp);
	if(s_afp) fclose(s_afp);
	fclose(file.fp);
}
