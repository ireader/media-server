#include "mov-reader.h"
#include "mov-format.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static uint8_t s_packet[2 * 1024 * 1024];
static uint8_t s_buffer[4 * 1024 * 1024];
static FILE *s_vfp, *s_afp;
static struct mpeg4_avc_t s_avc;
static struct mpeg4_aac_t s_aac;

extern "C" size_t mpeg4_mp4toannexb(const struct mpeg4_avc_t* avc, const void* data, size_t bytes, void* out, size_t size);

static void onread(void* flv, int avtype, const void* buffer, size_t bytes, int64_t pts, int64_t dts)
{
	if (MOV_AVC1 == avtype)
	{
		int n = mpeg4_mp4toannexb(&s_avc, buffer, bytes, s_packet, sizeof(s_packet));
		fwrite(s_packet, 1, n, s_vfp);
	}
	else if (MOV_MP4A == avtype)
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

static void mov_video_info(void* /*param*/, int /*avtype*/, int /*width*/, int /*height*/, const void* extra, size_t bytes)
{
	mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s_avc);

	// write sps/pps
	int n = mpeg4_avc_to_nalu(&s_avc, (uint8_t*)s_buffer, sizeof(s_buffer));
	fwrite(s_buffer, 1, n, s_vfp);
}

static void mov_audio_info(void* /*param*/, int /*avtype*/, int channel_count, int /*bit_per_sample*/, int sample_rate, const void* /*extra*/, size_t /*bytes*/)
{
	s_aac.profile = MPEG4_AAC_LC;
	s_aac.channel_configuration = channel_count;
	s_aac.sampling_frequency_index = mpeg4_aac_audio_frequency_from(sample_rate);
}

void mov_reader_test(const char* mp4)
{
	s_vfp = fopen("v.h264", "wb");
	s_afp = fopen("a.aac", "wb");

	mov_reader_t* mov = mov_reader_create(mp4);

	mov_reader_getinfo(mov, mov_video_info, mov_audio_info, NULL);

	while (mov_reader_read(mov, s_buffer, sizeof(s_buffer), onread, NULL) > 0)
	{
	}

	mov_reader_destroy(mov);
	fclose(s_vfp);
	fclose(s_afp);
}
