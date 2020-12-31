#include "flv-writer.h"
#include "flv-muxer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct h264_raw_t
{
	flv_muxer_t* flv;
	uint32_t pts, dts;
	const uint8_t* ptr;
};

extern "C" void mpeg4_h264_annexb_nalu(const void* h264, size_t bytes, void(*handler)(void* param, const void* nalu, size_t bytes), void* param);

static int on_flv_packet(void* flv, int type, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, type, data, bytes, timestamp);
}

static void h265_handler(void* param, const void* nalu, size_t bytes)
{
	struct h264_raw_t* ctx = (struct h264_raw_t*)param;
	assert(ctx->ptr < nalu);

	const uint8_t* end = (const uint8_t*)nalu + bytes;
	uint8_t nalutype = (*(uint8_t*)nalu) & 0x1f;
	if (1 <= nalutype && nalutype <= 5)
	{
		flv_muxer_hevc(ctx->flv, ctx->ptr, end - ctx->ptr, ctx->pts, ctx->dts);
		ctx->ptr = end;
		ctx->pts += 40;
		ctx->dts += 40;
	}
}

void hevc2flv_test(const char* inputH265, const char* outputFLV)
{
	struct h264_raw_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	void* f = flv_writer_create(outputFLV);
	ctx.flv = flv_muxer_create(on_flv_packet, f);
	FILE* fp = fopen(inputH265, "rb");

	static uint8_t buffer[4 * 1024 * 1024];
	size_t n = fread(buffer, 1, sizeof(buffer), fp);
	ctx.ptr = buffer;
	mpeg4_h264_annexb_nalu(buffer, n, h265_handler, &ctx);
	fclose(fp);

	flv_muxer_destroy(ctx.flv);
	flv_writer_destroy(f);
}
