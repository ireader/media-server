#define RTP_DEMUXER 0

#include "rtp-dump.h"
#if RTP_DEMUXER
#include "rtp-demuxer.h"
#else
#include "rtsp-demuxer.h"
#endif
#include "avpkt2bs.h"
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>


struct rtp_dump_test_t
{
#if RTP_DEMUXER
	struct rtp_demuxer_t* demuxer;
#else
	struct rtsp_demuxer_t* demuxer;
	struct avpkt2bs_t bs;
#endif

	FILE* fp;
};

#if RTP_DEMUXER
static int rtp_onpacket(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
	int r;
	struct rtp_dump_test_t* ctx = (struct rtp_dump_test_t*)param;
	r = (int)fwrite(packet, 1, bytes, ctx->fp);
	assert(r == bytes);
	return r == bytes ? 0 : ferror(ctx->fp);
}
#else
static int rtsp_onpacket(void* param, struct avpacket_t* pkt)
{
	int r;
	struct rtp_dump_test_t* ctx = (struct rtp_dump_test_t*)param;

	static int64_t s_dts[8] = { 0 };
	if (0 == s_dts[pkt->stream->stream])
		s_dts[pkt->stream->stream] = pkt->dts;
	printf("[%d:0x%x] pts: %" PRId64 ", dts: %" PRId64 ", cts: %" PRId64 ", diff: %" PRId64 ", bytes: %d\n", pkt->stream->stream, (unsigned int)pkt->stream->codecid, pkt->pts, pkt->dts, pkt->pts - pkt->dts, pkt->dts - s_dts[pkt->stream->stream], pkt->size);
	s_dts[pkt->stream->stream] = pkt->dts;

	if (avstream_type(pkt->stream) != AVSTREAM_VIDEO)
		return 0;

	r = avpkt2bs_input(&ctx->bs, pkt);
	fwrite(ctx->bs.ptr, 1, r, ctx->fp);
	//r = (int)fwrite(pkt->data, 1, pkt->size, ctx->fp);
	//assert(r == pkt->size);
	return 0;
}
#endif

void rtp_dump_test(const char* file)
{
	int r;
	uint8_t data[1500];
	uint32_t clock;
	struct rtpdump_t* dump;
	FILE* fp;

	struct rtp_dump_test_t ctx;
	ctx.fp = fopen("rtp.bin", "wb");
	
	dump = rtpdump_open(file, 0);
#if RTP_DEMUXER
	ctx.demuxer = rtp_demuxer_create(100, 90000, 100, "MP2P", rtp_onpacket, &ctx);
#else
	avpkt2bs_create(&ctx.bs);
	ctx.demuxer = rtsp_demuxer_create(0, 100, rtsp_onpacket, &ctx);
	//r = rtsp_demuxer_add_payload(ctx.demuxer, 90000, 96, "PS", "");
	r = rtsp_demuxer_add_payload(ctx.demuxer, 90000, 100, "H264", "100 profile-level-id=420028;sprop-parameter-sets=Z0IAKOkBQHsg,aM44gA==");
	//r = rtsp_demuxer_add_payload(ctx.demuxer, 90000, 99, "H264", "99 packetization-mode=1;profile-level-id=4D4033; sprop-parameter-sets=Z01AM5pkAeACH/4C3AQEBQAAAwPoAAB1MOhgAJ/8AAE/8i7y40MABP/gAAn/kXeXCgA=,aO44gA==");
    //r = rtsp_demuxer_add_payload(ctx.demuxer, 44100, 121, "MP4A-LATM", "121 config=4000242000;cpresent=0;object=2;profile-level-id=1");
	assert(0 == r);
#endif
	while (1)
	{
		r = rtpdump_read(dump, &clock, data, sizeof(data));
		if (r <= 0)
			break;

#if RTP_DEMUXER
		r = rtp_demuxer_input(ctx.demuxer, data, r);
#else
		r = rtsp_demuxer_input(ctx.demuxer, data, r);
#endif
		assert(r >= 0);
	}

#if RTP_DEMUXER
	rtp_demuxer_destroy(&ctx.demuxer);
#else
	rtsp_demuxer_destroy(ctx.demuxer);
	avpkt2bs_destroy(&ctx.bs);
#endif
	rtpdump_close(dump);
	fclose(ctx.fp);
}
