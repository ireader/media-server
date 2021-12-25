#include "flv-writer.h"
#include "flv-muxer.h"
#include "aom-av1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct av1_raw_t
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

void av1toflv_test(const char* obu, const char* outputFLV)
{
	struct av1_raw_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	void* f = flv_writer_create(outputFLV);
	ctx.flv = flv_muxer_create(on_flv_packet, f);
	FILE* fp = fopen(obu, "rb");

	static uint8_t buffer[4 * 1024 * 1024];
	while (1)
	{
		uint8_t ptr[2];
		if(sizeof(ptr) != fread(ptr, 1, sizeof(ptr), fp))
			break;

		size_t len = (ptr[0] << 8) | ptr[1];
		assert(len <= sizeof(buffer));
		if (len != fread(buffer, 1, len, fp))
			break;

		flv_muxer_av1(ctx.flv, buffer, len, ctx.pts, ctx.dts);
		ctx.pts += 40;
		ctx.dts += 40;
	}
	fclose(fp);

	flv_muxer_destroy(ctx.flv);
	flv_writer_destroy(f);
}
