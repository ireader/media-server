#include "mov-writer.h"
#include "mov-format.h"
#include "mpeg4-aac.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

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

static void g711_read_frame(mov_writer_t* mov, const uint8_t* ptr, const uint8_t* end)
{
	int track = -1;
	int64_t pts = 0;

	while (ptr < end)
	{
		if (-1 == track)
		{
			track = mov_writer_add_audio(mov, MOV_OBJECT_G711a, 1, 16, 8000, NULL, 0);
			if(-1 == track) continue;
		}

		int n = ptr + 320 < end ? 320 : end - ptr;
		mov_writer_write(mov, track, ptr, n, pts, pts, 0);
		pts += n / 8;
		ptr += n;
	}
}

static void aac_read_frame(mov_writer_t* mov, const uint8_t* ptr, const uint8_t* end)
{
	int rate = 1;
	int track = -1;
	int64_t pts = 0;
	uint64_t samples = 0;
	struct mpeg4_aac_t aac;
	uint8_t extra_data[64 * 1024];

	while (ptr + 7 < end)
	{
		mpeg4_aac_adts_load(ptr, end - ptr, &aac);
		if (-1 == track)
		{
			int extra_data_size = mpeg4_aac_audio_specific_config_save(&aac, extra_data, sizeof(extra_data));
			rate = mpeg4_aac_audio_frequency_to((enum mpeg4_aac_frequency)aac.sampling_frequency_index);
			track = mov_writer_add_audio(mov, MOV_OBJECT_AAC, aac.channel_configuration, 16, rate, extra_data, extra_data_size);
			if (-1 == track) continue;
		}

		samples += 1024; // aac frame
		int framelen = ((ptr[3] & 0x03) << 11) | (ptr[4] << 3) | (ptr[5] >> 5);
		mov_writer_write(mov, track, ptr + 7, framelen - 7, pts, pts, 0);
		pts = samples * 1000 / rate;
		ptr += framelen;
	}
}

void mov_writer_audio(const char* audio, int type, const char* mp4)
{
	long bytes = 0;
	uint8_t* ptr = file_read(audio, &bytes);
	if (NULL == ptr) return;

	FILE* fp = fopen(mp4, "wb+");
	mov_writer_t* mov = mov_writer_create(mov_file_buffer(), fp, MOV_FLAG_FASTSTART);
	switch (type)
	{
	case 1:
		aac_read_frame(mov, ptr, ptr + bytes);
		break;
	case 2:
		g711_read_frame(mov, ptr, ptr + bytes);
		break;
	default:
		assert(0);
	}
	mov_writer_destroy(mov);
	fclose(fp);
	free(ptr);
}
