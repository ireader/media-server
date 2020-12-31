#include "flv-writer.h"
#include "flv-muxer.h"
#include "mpeg4-avc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct h264_raw_t
{
	flv_muxer_t* flv;
	uint32_t pts, dts;
	const uint8_t* ptr;
    int vcl;
};

static int on_flv_packet(void* flv, int type, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, type, data, bytes, timestamp);
}

static void h264_handler(void* param, const uint8_t* nalu, int bytes)
{
	struct h264_raw_t* ctx = (struct h264_raw_t*)param;
	assert(ctx->ptr < nalu);

    const uint8_t* ptr = nalu - 3;
//    const uint8_t* end = (const uint8_t*)nalu + bytes;
    uint8_t nalutype = nalu[0] & 0x1f;
    if (ctx->vcl > 0 && h264_is_new_access_unit((const uint8_t*)nalu, bytes))
    {
        flv_muxer_avc(ctx->flv, ctx->ptr, ptr - ctx->ptr, ctx->pts, ctx->dts);
        ctx->pts += 40;
        ctx->dts += 40;

        ctx->ptr = ptr;
        ctx->vcl = 0;
    }
    
    if (1 <= nalutype && nalutype <= 5)
        ++ctx->vcl;
}

void avc2flv_test(const char* inputH264, const char* outputFLV)
{
	struct h264_raw_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	void* f = flv_writer_create(outputFLV);
	ctx.flv = flv_muxer_create(on_flv_packet, f);
	FILE* fp = fopen(inputH264, "rb");

	static uint8_t buffer[32 * 1024 * 1024];
	size_t n = fread(buffer, 1, sizeof(buffer), fp);
	ctx.ptr = buffer;
	mpeg4_h264_annexb_nalu(buffer, n, h264_handler, &ctx);
	fclose(fp);

	flv_muxer_destroy(ctx.flv);
	flv_writer_destroy(f);
}
