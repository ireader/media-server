#include "mp4-writer.h"
#include "mov-format.h"
#include "webm-vpx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static uint8_t s_buffer[2 * 1024 * 1024];
static uint8_t s_extra_data[64 * 1024];

struct mov_vpx_test_t
{
	mp4_writer_t* mov;
	struct webm_vpx_t vpx;

	int track;
	int width;
	int height;
	uint32_t pts, dts;
	uint8_t* ptr;
	int bytes;

	int vcl;
	int keyframe;
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

static int vpx_write(struct mov_vpx_test_t* ctx, const void* data, int bytes)
{
	if (ctx->track < 0)
	{
		int r = webm_vpx_codec_configuration_record_from_vp9(&ctx->vpx, &ctx->width, &ctx->height, data, bytes);
		if (ctx->width < 1 || ctx->height < 1)
		{
			//ctx->ptr = end;
			return -2; // waiting for sps/pps
		}

		int extra_data_size = webm_vpx_codec_configuration_record_save(&ctx->vpx, s_extra_data, sizeof(s_extra_data));
		if (extra_data_size <= 0)
		{
			// invalid AVCC
			assert(0);
			return -1;
		}

		// TODO: waiting for key frame ???
		ctx->track = mp4_writer_add_video(ctx->mov, MOV_OBJECT_VP9, ctx->width, ctx->height, s_extra_data, extra_data_size);
		if (ctx->track < 0)
			return -1;
	}

	mp4_writer_write(ctx->mov, ctx->track, data, bytes, ctx->pts, ctx->pts, ctx->keyframe ? MOV_AV_FLAG_KEYFREAME : 0);
	ctx->pts += 40;
	ctx->dts += 40;
	return 0;
}

static int vpx_handler(struct mov_vpx_test_t* ctx, const uint8_t* ptr, size_t len)
{
	while (len > 2)
	{
		size_t n = (((size_t)ptr[0]) << 8) | ptr[1];
		ptr += 2;
		len -= 2;
		if (n > len)
			return -1;

		vpx_write(ctx, ptr, n);
		ptr += n;
		len -= n;
	}
	return 0;
}

// file format:
// 2ByteLenght(MSB) + vp9 payload + 2ByteLenght(MSB) + vp9 payload + ...
void mov_writer_vp9(const char* file, int width, int height, const char* mp4)
{
	struct mov_vpx_test_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.track = -1;
	ctx.width = width;
	ctx.height = height;
	ctx.ptr = s_buffer;

	long bytes = 0;
	uint8_t* ptr = file_read(file, &bytes);
	if (NULL == ptr) return;

	FILE* fp = fopen(mp4, "wb+");
	ctx.mov = mp4_writer_create(0, mov_file_buffer(), fp, 0);
	vpx_handler(&ctx, ptr, bytes);
	mp4_writer_destroy(ctx.mov);

	fclose(fp);
	free(ptr);
}
