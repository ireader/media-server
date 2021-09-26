#include "mov-writer.h"
#include "mov-format.h"
#include "aom-av1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

enum
{
	OBU_SEQUENCE_HEADER = 1,
	OBU_TEMPORAL_DELIMITER = 2,
	OBU_FRAME = 6,
};

static uint8_t s_buffer[2 * 1024 * 1024];
static uint8_t s_extra_data[64 * 1024];

struct mov_av1_test_t
{
	mov_writer_t* mov;
	struct aom_av1_t av1;

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

static int av1_write(struct mov_av1_test_t* ctx, const void* data, int bytes)
{
	if (ctx->track < 0)
	{
		int r = aom_av1_codec_configuration_record_init(&ctx->av1, data, bytes);
		if (ctx->av1.width < 1 || ctx->av1.height < 1)
		{
			//ctx->ptr = end;
			return -2; // waiting for sps/pps
		}

		int extra_data_size = aom_av1_codec_configuration_record_save(&ctx->av1, s_extra_data, sizeof(s_extra_data));
		if (extra_data_size <= 0)
		{
			// invalid AVCC
			assert(0);
			return -1;
		}

		// TODO: waiting for key frame ???
		ctx->track = mov_writer_add_video(ctx->mov, MOV_OBJECT_AV1, ctx->width, ctx->height, s_extra_data, extra_data_size);
		if (ctx->track < 0)
			return -1;
	}

	mov_writer_write(ctx->mov, ctx->track, data, bytes, ctx->pts, ctx->pts, ctx->keyframe ? MOV_AV_FLAG_KEYFREAME : 0);
	ctx->pts += 40;
	ctx->dts += 40;
	return 0;
}

static int av1_handler(void* param, const uint8_t* obu, size_t bytes)
{
	struct mov_av1_test_t* ctx = (struct mov_av1_test_t*)param;
	
	uint8_t obu_type = (obu[0] >> 3) & 0x0F;
	if (ctx->vcl > 0 && OBU_TEMPORAL_DELIMITER == obu_type)
	{
		int r = av1_write(ctx, ctx->ptr, ctx->bytes);
		if (-1 == r)
			return 0; // wait for more data

		ctx->bytes = 0;
		ctx->vcl = 0;
		ctx->keyframe = 0;
	}

	if (OBU_TEMPORAL_DELIMITER == obu_type)
		return 0; // ignore
	if (OBU_SEQUENCE_HEADER == obu_type)
		ctx->keyframe = 1;
	if (OBU_FRAME == obu_type)
		++ctx->vcl;

	memcpy(ctx->ptr + ctx->bytes, obu, bytes);
	ctx->bytes += bytes;
	return 0;
}

void mov_writer_av1(const char* obu, int width, int height, const char* mp4)
{
	struct mov_av1_test_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.track = -1;
	ctx.width = width;
	ctx.height = height;
	ctx.ptr = s_buffer;

	long bytes = 0;
	uint8_t* ptr = file_read(obu, &bytes);
	if (NULL == ptr) return;

	FILE* fp = fopen(mp4, "wb+");
	ctx.mov = mov_writer_create(mov_file_buffer(), fp, MOV_FLAG_FASTSTART);
	aom_av1_obu_split(ptr, bytes, av1_handler, &ctx);
	mov_writer_destroy(ctx.mov);

	fclose(fp);
	free(ptr);
}
