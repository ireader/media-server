#include "mkv-writer.h"
#include "mkv-format.h"
#include "mpeg4-aac.h"
#include "riff-acm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static uint8_t* file_read(const char* file, long* size)
{
	FILE* fp = fopen(file, "rb");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		*size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		uint8_t* ptr = (uint8_t*)malloc(*size);
		fread(ptr, 1, *size, fp);
		fclose(fp);

		return ptr;
	}

	return NULL;
}

static void g711_read_frame(mkv_writer_t* mkv, const uint8_t* ptr, const uint8_t* end)
{
	int track = -1;
	int64_t pts = 0;

	while (ptr < end)
	{
		if (-1 == track)
		{
			struct wave_format_t wav;
			memset(&wav, 0, sizeof(wav));
			wav.wFormatTag = WAVE_FORMAT_ALAW;
			wav.nChannels = 1;
			wav.nSamplesPerSec = 8000;
			wav.nAvgBytesPerSec = 8000;
			wav.nBlockAlign = 1;
			wav.wBitsPerSample = 8;
			uint8_t data[18];
			int n = wave_format_save(&wav, data, sizeof(data));
			track = mkv_writer_add_audio(mkv, MKV_CODEC_AUDIO_ACM, 1, 16, 8000, data, n);
			if (-1 == track) continue;
		}

		int n = ptr + 320 < end ? 320 : end - ptr;
		mkv_writer_write(mkv, track, ptr, n, pts, pts, 0);
		pts += n / 8; // 8000Hz/8-bits/1-channel
		ptr += n;
	}
}

static void aac_read_frame(mkv_writer_t* mkv, const uint8_t* ptr, const uint8_t* end)
{
	int rate = 1;
	int track = -1;
	int64_t pts = 0;
	uint64_t samples = 1024; // aac frame
	struct mpeg4_aac_t aac;
	uint8_t extra_data[64 * 1024];

	while (ptr + 7 < end)
	{
		mpeg4_aac_adts_load(ptr, end - ptr, &aac);
		if (-1 == track)
		{
			int extra_data_size = mpeg4_aac_audio_specific_config_save(&aac, extra_data, sizeof(extra_data));
			rate = mpeg4_aac_audio_frequency_to((enum mpeg4_aac_frequency)aac.sampling_frequency_index);
			track = mkv_writer_add_audio(mkv, MKV_CODEC_AUDIO_AAC, aac.channel_configuration, 16, rate, extra_data, extra_data_size);
			if (-1 == track) continue;
			assert(rate != 0);
		}

		int framelen = mpeg4_aac_adts_frame_length(ptr, end - ptr);
		mkv_writer_write(mkv, track, ptr + 7, framelen - 7, pts, pts, 0);
		pts += samples * 1000 / rate;
		ptr += framelen;
	}
}

void mkv_writer_audio(const char* audio, int type, const char* out)
{
	long bytes = 0;
	uint8_t* ptr = file_read(audio, &bytes);
	if (NULL == ptr) return;

	struct mkv_file_cache_t wfile;
	memset(&wfile, 0, sizeof(wfile));
	wfile.fp = fopen(out, "wb+");

	mkv_writer_t* mkv = mkv_writer_create(mkv_file_cache_buffer(), &wfile, 0);
	switch (type)
	{
	case 1:
		aac_read_frame(mkv, ptr, ptr + bytes);
		break;
	case 2:
		g711_read_frame(mkv, ptr, ptr + bytes);
		break;
	default:
		assert(0);
	}
	mkv_writer_destroy(mkv);
	fclose(wfile.fp);
	free(ptr);
}
