// https://wiki.wireshark.org/rtpdump
// https://code.wireshark.org/review/gitweb?p=wireshark.git;a=blob;f=ui/tap-rtp-common.c

#include "rtp-dump.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "byte-order.h"

struct rtpdump_t
{
	FILE* fp;
	uint32_t start_sec;
	uint32_t start_usec;
	uint32_t source;
	uint16_t port;
};

static int rtmp_dump_read_file_header(struct rtpdump_t* ctx)
{
	int i;
	uint8_t buf[64];

	// #!rtpplay[[version]] [[addr]]/[[port]]\n
	for (i = 0; i < sizeof(buf); i++)
	{
		fread(buf + i, 1, 1, ctx->fp);
		if (buf[i] == '\n')
			break;
	}

	if (i >= sizeof(buf) || 1 != fread(buf, 16, 1, ctx->fp))
		return -1;

	be_read_uint32(buf, &ctx->start_sec);
	be_read_uint32(buf + 4, &ctx->start_usec);
	be_read_uint32(buf + 8, &ctx->source);
	be_read_uint16(buf + 12, &ctx->port);
	return 0;
}

struct rtpdump_t* rtpdump_open(const char* file, int flags)
{
	FILE* fp;
	struct rtpdump_t* ctx;

	fp = fopen(file, "rb");
	if (!fp)
		return NULL;

	ctx = (struct rtpdump_t*)calloc(1, sizeof(*ctx));
	if (!ctx)
	{
		fclose(fp);
		return NULL;
	}

	ctx->fp = fp;
	if (0 != rtmp_dump_read_file_header(ctx))
	{
		rtpdump_close(ctx);
		return NULL;
	}
	return ctx;
}

int rtpdump_close(struct rtpdump_t* ctx)
{
	if (ctx->fp)
	{
		fclose(ctx->fp);
		ctx->fp = NULL;
	}

	free(ctx);
	return 0;
}

int rtpdump_read(struct rtpdump_t* ctx, uint32_t* clock, void* data, int bytes)
{
	uint8_t buf[8];
	uint16_t len; /* length of packet, including this header (may be smaller than plen if not whole packet recorded) */
	uint16_t payload; /* actual header+payload length for RTP, 0 for RTCP */
	
	if (1 != fread(buf, 8, 1, ctx->fp))
		return -1;

	be_read_uint16(buf, &len);
	be_read_uint16(buf + 2, &payload);
	be_read_uint32(buf + 4, clock); /* milliseconds since the start of recording */

	if (bytes < len - 8)
		return -1;

	if (1 != fread(data, len - 8, 1, ctx->fp))
		return -1;

	return len - 8;
}
