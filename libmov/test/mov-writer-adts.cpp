#include "mov-writer.h"
#include "mov-format.h"
#include "mpeg4-aac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static uint8_t s_buffer[2 * 1024 * 1024];
static uint8_t s_extra_data[64 * 1024];

struct mov_adts_test_t
{
	mov_writer_t* mov;
	struct mpeg4_aac_t aac;

	int track;
	uint32_t pts, dts;
	const uint8_t* ptr;

	int vcl;
};

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

static void adts_reader(struct mov_adts_test_t* ctx, const uint8_t* ptr, size_t bytes)
{
	int64_t pts = 0;
	while(ptr && bytes > 7)
	{
		int n = mpeg4_aac_adts_frame_length(ptr, bytes);
		if (n < 0)
			break;

		if (n > bytes)
			break;

		if (-1 == ctx->track)
		{
			uint8_t asc[16];
			assert(7 == mpeg4_aac_adts_load(ptr, bytes, &ctx->aac));
			int len = mpeg4_aac_audio_specific_config_save(&ctx->aac, asc, sizeof(asc));
			assert(len > 0 && len <= sizeof(asc));
			ctx->track = mov_writer_add_audio(ctx->mov, MOV_OBJECT_AAC, ctx->aac.channels, 16, ctx->aac.sampling_frequency, asc, len);
			assert(ctx->track >= 0);
		}

		assert(0 == mov_writer_write(ctx->mov, ctx->track, ptr + 7, n - 7, pts, pts, 0));
		ptr += n;
		bytes -= n;
		pts += 1024 /*samples*/ * 1000 / ctx->aac.sampling_frequency;
	}
}

void mov_writer_adts_test(const char* file)
{
	struct mov_adts_test_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.track = -1;

	long bytes = 0;
	uint8_t* ptr = file_read(file, &bytes);
	if (NULL == ptr) return;
	ctx.ptr = ptr;

	FILE* fp = fopen("adts.mp4", "wb+");
	ctx.mov = mov_writer_create(mov_file_buffer(), fp, 0);
	adts_reader(&ctx, ptr, bytes);
	mov_writer_destroy(ctx.mov);

	fclose(fp);
	free(ptr);
}
