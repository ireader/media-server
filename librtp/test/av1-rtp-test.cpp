#include "aom-av1.h"
#include "rtp-payload.h"
#include "rtp-profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

enum
{
	OBU_SEQUENCE_HEADER = 1,
	OBU_TEMPORAL_DELIMITER = 2,
	OBU_FRAME = 6,
};

struct av1_rtp_test_t
{
	struct aom_av1_t av1;
	void* encoder;
	void* decoder;

	uint32_t pts, dts;
	uint8_t* ptr;
	int bytes;

	int vcl;
	int keyframe;

	FILE* wfp;
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

static void* rtp_alloc(void* /*param*/, int bytes)
{
	static uint8_t buffer[2 * 1024 * 1024 + 4] = { 0, 0, 0, 1, };
	assert(bytes <= sizeof(buffer) - 4);
	return buffer + 4;
}

static void rtp_free(void* /*param*/, void* /*packet*/)
{
}

static int rtp_encode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int /*flags*/)
{
	struct av1_rtp_test_t* ctx = (struct av1_rtp_test_t*)param;

	//int x = rand();
	//if( (x % 100) < RTP_LOST_PERCENT )
	//{
	//    printf("======== discard [%s] timestamp: %u ==============\n", ctx->av ? "V" : "A", (unsigned int)timestamp);
	//    return 0;
	//}

	int r = rtp_payload_decode_input(ctx->decoder, packet, bytes);
	return r >= 0 ? 0 : r;
}

static int rtp_decode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
	struct av1_rtp_test_t* ctx = (struct av1_rtp_test_t*)param;
	printf("RTP Decode: timestamp: %u, bytes: %d\n", (unsigned int)timestamp, bytes);

	static const uint8_t av1_temporal_delimiter[] = { 0x12, 0x00 };
	assert(sizeof(av1_temporal_delimiter) == fwrite(av1_temporal_delimiter, 1, sizeof(av1_temporal_delimiter), ctx->wfp));
	assert(bytes == fwrite(packet, 1, bytes, ctx->wfp));
	return 0;
}

static int av1_handler(void* param, const uint8_t* obu, size_t bytes)
{
	struct av1_rtp_test_t* ctx = (struct av1_rtp_test_t*)param;

	uint8_t obu_type = (obu[0] >> 3) & 0x0F;
	if (ctx->vcl > 0 && OBU_TEMPORAL_DELIMITER == obu_type)
	{
		printf("av1 frame: %u, bytes: %d\n", (unsigned int)ctx->dts, ctx->bytes);
		assert(0 == rtp_payload_encode_input(ctx->encoder, ctx->ptr, ctx->bytes, (unsigned int)ctx->dts));
		
		ctx->dts += 40; // fps 25
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

void av1_rtp_test(const char* low_overhead_bitstream_format_obu)
{
	struct av1_rtp_test_t ctx;
	memset(&ctx, 0, sizeof(ctx));

	static uint8_t s_buffer[2 * 1024 * 1024];
	snprintf((char*)s_buffer, sizeof(s_buffer), "%s.obu", low_overhead_bitstream_format_obu);
	ctx.wfp = fopen((const char*)s_buffer, "wb");
	ctx.ptr = s_buffer;

	struct rtp_payload_t handler1;
	handler1.alloc = rtp_alloc;
	handler1.free = rtp_free;
	handler1.packet = rtp_decode_packet;
	ctx.decoder = rtp_payload_decode_create(97, "AV1", &handler1, &ctx);

	struct rtp_payload_t handler2;
	handler2.alloc = rtp_alloc;
	handler2.free = rtp_free;
	handler2.packet = rtp_encode_packet;
	ctx.encoder = rtp_payload_encode_create(97, "AV1", 0, 0, &handler2, &ctx);

	long bytes = 0;
	uint8_t* ptr = file_read(low_overhead_bitstream_format_obu, &bytes);
	if (NULL == ptr) return;

	aom_av1_obu_split(ptr, bytes, av1_handler, &ctx);

	free(ptr);
	fclose(ctx.wfp);
}
