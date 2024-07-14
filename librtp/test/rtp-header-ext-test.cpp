#include "rtp-payload.h"
#include "rtp-profile.h"
#include "rtp-header.h"
extern "C" {
#include "rtp-packet.h"
}
#include "rtp-ext.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64)
#define strcasecmp _stricmp
#endif

struct rtp_header_ext_test_t
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

static void rtp_free(void* /*param*/, void* /*packet*/)
{
}

static int rtp_encode_packet(void* param, const void* packet, int bytes, uint32_t /*timestamp*/, int /*flags*/)
{
	struct rtp_payload_test_t* ctx = (struct rtp_payload_test_t*)param;
	//uint8_t size[2];
	//size[0] = (uint8_t)((uint32_t)bytes >> 8);
	//size[1] = (uint8_t)(uint32_t)bytes;
	//fwrite(size, 1, sizeof(size), ctx->frtp2);
	//fwrite(packet, 1, bytes, ctx->frtp2);
	return 0;
}

static int rtp_decode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
	//memcpy(buffer + size, packet, bytes);
	//size += bytes;

	//// TODO:
	//// check media file
	//fwrite(buffer, 1, size, ctx->fsource2);

	//return rtp_payload_encode_input(ctx->encoder, buffer, size, timestamp);
	return 0;
}

void rtp_header_ext_test(const char* rtpfile)
{
	struct rtp_header_ext_test_t ctx;

	ctx.frtp = fopen(rtpfile, "rb");

	rtp_packet_setsize(1456); // 1456(live555)

	//struct rtp_payload_t handler2;
	//handler2.alloc = rtp_alloc;
	//handler2.free = rtp_free;
	//handler2.packet = rtp_encode_packet;
	//ctx.encoder = rtp_payload_encode_create(payload, encoding, seq, ssrc, &handler2, &ctx);

	//struct rtp_payload_t handler1;
	//handler1.alloc = rtp_alloc;
	//handler1.free = rtp_free;
	//handler1.packet = rtp_decode_packet;
	//ctx.decoder = rtp_payload_decode_create(payload, encoding, &handler1, &ctx);

	const struct rtp_ext_uri_t* audio[5] = {
		NULL,
		rtp_ext_find_uri("urn:ietf:params:rtp-hdrext:ssrc-audio-level"),
		rtp_ext_find_uri("http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time"),
		rtp_ext_find_uri("http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01"),
		rtp_ext_find_uri("urn:ietf:params:rtp-hdrext:sdes:mid"),
	};
	const struct rtp_ext_uri_t* video[16] = {
		NULL,
		NULL,
		rtp_ext_find_uri("http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time"),
		rtp_ext_find_uri("http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01"),
		rtp_ext_find_uri("urn:ietf:params:rtp-hdrext:sdes:mid"),
		rtp_ext_find_uri("http://www.webrtc.org/experiments/rtp-hdrext/playout-delay"),
		rtp_ext_find_uri("http://www.webrtc.org/experiments/rtp-hdrext/video-content-type"),
		rtp_ext_find_uri("http://www.webrtc.org/experiments/rtp-hdrext/video-timing"),
		rtp_ext_find_uri("http://www.webrtc.org/experiments/rtp-hdrext/color-space"),
		NULL,
		rtp_ext_find_uri("urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id"),
		rtp_ext_find_uri("urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id"),
		NULL,
		rtp_ext_find_uri("urn:3gpp:video-orientation"),
		rtp_ext_find_uri("urn:ietf:params:rtp-hdrext:toffset"),
	};

	for (int i = 0; 1; i++)
	{
		uint8_t s2[2];
		if (2 != fread(s2, 1, 2, ctx.frtp))
			break;

		//ctx.size = (s2[0] << 8) | s2[1];
		ctx.size = (s2[1] << 8) | s2[0];
		//ctx.size -= 4;
		assert(ctx.size < sizeof(ctx.packet));
		if (ctx.size != (int)fread(ctx.packet, 1, ctx.size, ctx.frtp))
			break;

		rtp_packet_t pkt;
		rtp_packet_deserialize(&pkt, ctx.packet, ctx.size);
		printf("[%d] ssrc: 0x%04x, size: %d\n", i, pkt.rtp.ssrc, (int)ctx.size);
		if (pkt.extlen > 0)
		{
			struct rtp_ext_data_t data[256];
			rtp_ext_read(pkt.extprofile, (const uint8_t*)pkt.extension, pkt.extlen, data);
		}
		//rtp_payload_decode_input(ctx.decoder, ctx.packet, ctx.size);
	}

	fclose(ctx.frtp);
	//rtp_payload_decode_destroy(ctx.decoder);
	//rtp_payload_encode_destroy(ctx.encoder);
}
