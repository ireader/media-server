#include "rtp-payload.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct rtp_payload_test_t
{
	int payload;
	const char* encoding;

	FILE* frtp;
	FILE* fsource;
	FILE* frtp2;
	FILE* fsource2;

	void* encoder;
	void* decoder;

	size_t size;
	uint8_t packet[64 * 1024];
};

static void* rtp_alloc(void* /*param*/, int bytes)
{
	static uint8_t buffer[2 * 1024 * 1024 + 4] = { 0, 0, 0, 1, };
	assert(bytes <= sizeof(buffer) - 4);
	return buffer + 4;
}

static void rtp_free(void* /*param*/, void * /*packet*/)
{
}

static void rtp_encode_packet(void* param, const void *packet, int bytes, uint32_t /*timestamp*/, int /*flags*/)
{
	struct rtp_payload_test_t* ctx = (struct rtp_payload_test_t*)param;
	uint8_t size[2];
	size[0] = (uint8_t)((uint32_t)bytes >> 8);
	size[1] = (uint8_t)(uint32_t)bytes;
	fwrite(size, 1, sizeof(size), ctx->frtp2);
	fwrite(packet, 1, bytes, ctx->frtp2);
}

static void rtp_decode_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
	static const uint8_t start_code[4] = { 0, 0, 0, 1 };
	struct rtp_payload_test_t* ctx = (struct rtp_payload_test_t*)param;
	
	static uint8_t buffer[2 * 1024 * 1024];
	assert(bytes + 4 < sizeof(buffer));
	assert(0 == flags);

	size_t size = 0;
	if (0 == strcmp("H264", ctx->encoding) || 0 == strcmp("H265", ctx->encoding))
	{
		memcpy(buffer, start_code, sizeof(start_code));
		size += sizeof(start_code);
	}
	memcpy(buffer + size, packet, bytes);
	size += bytes;

	// TODO:
	// check media file
	fwrite(buffer, 1, size, ctx->fsource2);

	rtp_payload_encode_input(ctx->encoder, buffer, size, timestamp);
}

static void rtp_payload_test(int payload, const char* encoding, uint16_t seq, uint32_t ssrc, const char* rtpfile, const char* sourcefile)
{
	struct rtp_payload_test_t ctx;
	ctx.payload = payload;
	ctx.encoding = encoding;

	ctx.frtp = fopen(rtpfile, "rb");
	ctx.fsource = fopen(sourcefile, "rb");
	ctx.frtp2 = fopen("out.rtp", "wb");
	ctx.fsource2 = fopen("out.media", "wb");

	rtp_packet_setsize(1444); // 1456(live555) - 12(RTP-Header)

	struct rtp_payload_t handler2;
	handler2.alloc = rtp_alloc;
	handler2.free = rtp_free;
	handler2.packet = rtp_encode_packet;
	ctx.encoder = rtp_payload_encode_create(payload, encoding, seq, ssrc, &handler2, &ctx);

	struct rtp_payload_t handler1;
	handler1.alloc = rtp_alloc;
	handler1.free = rtp_free;
	handler1.packet = rtp_decode_packet;
	ctx.decoder = rtp_payload_decode_create(payload, encoding, &handler1, &ctx);

	while (1)
	{
		uint8_t s2[2];
		if (2 != fread(s2, 1, 2, ctx.frtp))
			break;

		ctx.size = (s2[0] << 8) | s2[1];
		assert(ctx.size < sizeof(ctx.packet));
		if (ctx.size != (int)fread(ctx.packet, 1, ctx.size, ctx.frtp))
			break;

		rtp_payload_decode_input(ctx.decoder, ctx.packet, ctx.size);
	}

	fclose(ctx.frtp);
	fclose(ctx.fsource);
	fclose(ctx.frtp2);
	fclose(ctx.fsource2);
	rtp_payload_decode_destroy(ctx.decoder);
	rtp_payload_encode_destroy(ctx.encoder);
}

//void binary_diff(const char* f1, const char* f2);
void rtp_payload_test()
{
	//rtp_payload_test(33, "MP2T", 24470, 1726408532, "E:\\video\\rtp\\bipbop-gear1-all.ts.rtp", "E:\\video\\rtp\\bipbop-gear1-all.ts");
	//binary_diff("E:\\video\\rtp\\bipbop-gear1-all.ts.rtp", "out.rtp");
	//binary_diff("E:\\video\\rtp\\bipbop-gear1-all.ts", "out.media");

	//rtp_payload_test(96, "H264", 12686, 1957754144, "E:\\video\\rtp\\live555-test.h264.rtp", "E:\\video\\rtp\\live555-test.h264");
	//binary_diff("E:\\video\\rtp\\live555-test.h264.rtp", "out.rtp");
	//binary_diff("E:\\video\\rtp\\live555-test.h264", "out.media");

	//rtp_payload_test(96, "H265", 64791, 850623724, "E:\\video\\rtp\\live555-surfing.h265.rtp", "E:\\video\\rtp\\live555-surfing.h265");
	//binary_diff("E:\\video\\rtp\\live555-surfing.h265.rtp", "out.rtp");
	//binary_diff("E:\\video\\rtp\\live555-surfing.h265", "out.media");
}
