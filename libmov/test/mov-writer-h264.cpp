#include "mov-writer.h"
#include "mov-format.h"
#include "mpeg4-avc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

#define H264_NAL(v)	(v & 0x1F)

static uint8_t s_buffer[2 * 1024 * 1024];
static uint8_t s_extra_data[64 * 1024];

struct mov_h264_test_t
{
	mov_writer_t* mov;
	struct mpeg4_avc_t avc;

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

static int h264_write(struct mov_h264_test_t* ctx, const void* data, int bytes)
{
    int vcl = 0;
    int update = 0;
    int n = h264_annexbtomp4(&ctx->avc, data, bytes, s_buffer, sizeof(s_buffer), &vcl, &update);

    if (ctx->track < 0)
    {
        if (ctx->avc.nb_sps < 1 || ctx->avc.nb_pps < 1)
        {
            //ctx->ptr = end;
            return -2; // waiting for sps/pps
        }

        int extra_data_size = mpeg4_avc_decoder_configuration_record_save(&ctx->avc, s_extra_data, sizeof(s_extra_data));
        if (extra_data_size <= 0)
        {
            // invalid AVCC
            assert(0);
            return -1;
        }

        // TODO: waiting for key frame ???
        ctx->track = mov_writer_add_video(ctx->mov, MOV_OBJECT_H264, ctx->width, ctx->height, s_extra_data, extra_data_size);
        if (ctx->track < 0)
            return -1;
    }

    mov_writer_write(ctx->mov, ctx->track, s_buffer, n, ctx->pts, ctx->pts, 1 == vcl ? MOV_AV_FLAG_KEYFREAME : 0);
    ctx->pts += 40;
    ctx->dts += 40;
    return 0;
}

static void h264_handler(void* param, const uint8_t* nalu, int bytes)
{
	struct mov_h264_test_t* ctx = (struct mov_h264_test_t*)param;
	assert(ctx->ptr < nalu);

    const uint8_t* ptr = nalu - 3;
//	const uint8_t* end = (const uint8_t*)nalu + bytes;
	uint8_t nalutype = nalu[0] & 0x1f;
    if (ctx->vcl > 0 && h264_is_new_access_unit((const uint8_t*)nalu, bytes))
    {
        int r = h264_write(ctx, ctx->ptr, ptr - ctx->ptr);
        if (-1 == r)
            return; // wait for more data

        ctx->ptr = ptr;
        ctx->vcl = 0;
    }

	if (1 <= nalutype && nalutype <= 5)
        ++ctx->vcl;
}

void mov_writer_h264(const char* h264, int width, int height, const char* mp4)
{
	struct mov_h264_test_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.track = -1;
	ctx.width = width;
	ctx.height = height;

	long bytes = 0;
	uint8_t* ptr = file_read(h264, &bytes);
	if (NULL == ptr) return;
	ctx.ptr = ptr;

	FILE* fp = fopen(mp4, "wb+");
	ctx.mov = mov_writer_create(mov_file_buffer(), fp, MOV_FLAG_FASTSTART);
	mpeg4_h264_annexb_nalu(ptr, bytes, h264_handler, &ctx);
	mov_writer_destroy(ctx.mov);

	fclose(fp);
	free(ptr);
}
