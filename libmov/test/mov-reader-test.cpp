#include "mov-reader.h"
#include "mov-format.h"
#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static uint8_t s_packet[2 * 1024 * 1024];
static uint8_t s_buffer[4 * 1024 * 1024];
static FILE *s_vfp, *s_afp;
static struct mpeg4_hevc_t s_hevc;
static struct mpeg4_avc_t s_avc;
static struct mpeg4_aac_t s_aac;
static uint32_t s_aac_track = 0xFFFFFFFF;
static uint32_t s_avc_track = 0xFFFFFFFF;
static uint32_t s_hevc_track = 0xFFFFFFFF;

static void onread(void* flv, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts)
{
	if (s_avc_track == track)
	{
		int n = mpeg4_mp4toannexb(&s_avc, buffer, bytes, s_packet, sizeof(s_packet));
		fwrite(s_packet, 1, n, s_vfp);
	}
	else if (s_hevc_track == track)
	{
		int n = hevc_mp4toannexb(&s_hevc, buffer, bytes, s_packet, sizeof(s_packet));
		fwrite(s_packet, 1, n, s_vfp);
	}
	else if (s_aac_track == track)
	{
		uint8_t adts[32];
		int n = mpeg4_aac_adts_save(&s_aac, bytes, adts, sizeof(adts));
		fwrite(adts, 1, n, s_afp);
		fwrite(buffer, 1, bytes, s_afp);
	}
	else
	{
		assert(0);
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
	else
	{
		assert(0);
	}
}

static void mov_audio_info(void* /*param*/, uint32_t track, uint8_t object, int channel_count, int /*bit_per_sample*/, int sample_rate, const void* /*extra*/, size_t /*bytes*/)
{
	s_afp = fopen("a.aac", "wb");

	s_aac_track = track;
	assert(MOV_OBJECT_AAC == object);
	s_aac.profile = MPEG4_AAC_LC;
	s_aac.channel_configuration = channel_count;
	s_aac.sampling_frequency_index = mpeg4_aac_audio_frequency_from(sample_rate);
}

void mov_reader_test(const char* mp4)
{
	FILE* fp = fopen(mp4, "rb");
	mov_reader_t* mov = mov_reader_create(mov_file_buffer(), fp);
	uint64_t duration = mov_reader_getduration(mov);

	struct mov_reader_trackinfo_t info = { mov_video_info, mov_audio_info };
	mov_reader_getinfo(mov, &info, NULL);

	while (mov_reader_read(mov, s_buffer, sizeof(s_buffer), onread, NULL) > 0)
	{
	}

	duration /= 2;
	mov_reader_seek(mov, (int64_t*)&duration);

	mov_reader_destroy(mov);
	if(s_vfp) fclose(s_vfp);
	if(s_afp) fclose(s_afp);
	fclose(fp);
}
