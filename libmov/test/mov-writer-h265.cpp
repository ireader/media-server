#include "mov-writer.h"
#include "mov-format.h"
#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);
#define H265_NAL(v)	((v >> 1) & 0x3F)

static uint8_t s_buffer[2 * 1024 * 1024];
static uint8_t s_extra_data[64 * 1024];

struct mov_h265_test_t
{
	mov_writer_t* mov;
	struct mpeg4_hevc_t hevc;

	int track;
	int width;
	int height;
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

static int h265_write(struct mov_h265_test_t* ctx, const void* data, int bytes)
{
	int vcl = 0;
	int update = 0;
	int n = h265_annexbtomp4(&ctx->hevc, data, bytes, s_buffer, sizeof(s_buffer), &vcl, &update);

	if (ctx->track < 0)
	{
		if (ctx->hevc.numOfArrays < 1)
		{
			//ctx->ptr = end;
			return -2; // waiting for vps/sps/pps
		}

		int extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&ctx->hevc, s_extra_data, sizeof(s_extra_data));
		if (extra_data_size <= 0)
		{
			// invalid HVCC
			assert(0);
			return -1;
		}

		// TODO: waiting for key frame ???
		ctx->track = mov_writer_add_video(ctx->mov, MOV_OBJECT_HEVC, ctx->width, ctx->height, s_extra_data, extra_data_size);
		if (ctx->track < 0)
			return -1;
	}

	mov_writer_write(ctx->mov, ctx->track, s_buffer, n, ctx->pts, ctx->pts, 1 == vcl ? MOV_AV_FLAG_KEYFREAME : 0);
	ctx->pts += 40;
	ctx->dts += 40;
	return 0;
}

static void h265_handler(void* param, const uint8_t* nalu, int bytes)
{
	struct mov_h265_test_t* ctx = (struct mov_h265_test_t*)param;
	assert(ctx->ptr < nalu);

	const uint8_t* ptr = nalu - 3;
//	const uint8_t* end = (const uint8_t*)nalu + bytes;
	uint8_t nalutype = (nalu[0] >> 1) & 0x3f;
    if (ctx->vcl > 0 && h265_is_new_access_unit((const uint8_t*)nalu, bytes))
    {
        int r = h265_write(ctx, ctx->ptr, ptr - ctx->ptr);
        if (-1 == r)
            return; // wait for more data

        ctx->ptr = ptr;
        ctx->vcl = 0;
    }

	if (nalutype <= 31)
        ++ctx->vcl;
}

void mov_writer_h265(const char* h265, int width, int height, const char* mp4)
{
	struct mov_h265_test_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.track = -1;
	ctx.width = width;
	ctx.height = height;

	long bytes = 0;
	uint8_t* ptr = file_read(h265, &bytes);
	if (NULL == ptr) return;
	ctx.ptr = ptr;

	FILE* fp = fopen(mp4, "wb+");
	ctx.mov = mov_writer_create(mov_file_buffer(), fp, MOV_FLAG_FASTSTART);
	mpeg4_h264_annexb_nalu(ptr, bytes, h265_handler, &ctx);
	mov_writer_destroy(ctx.mov);

	fclose(fp);
	free(ptr);
}
