#include "rtp-dump.h"
#include "rtp-demuxer.h"
#include "rtsp-demuxer.h"
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>

#define RTP_DEMUXER 0

#if RTP_DEMUXER
static int rtp_onpacket(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
	int r;
	r = (int)fwrite(packet, 1, bytes, (FILE*)param);
	assert(r == bytes);
	return r == bytes ? 0 : ferror((FILE*)param);
}
#else
static int rtsp_onpacket(void* param, struct avpacket_t* pkt)
{
	int r;

	static int64_t s_dts = 0;
	if (0 == s_dts)
		s_dts = pkt->dts;
	printf("[%d:0x%x] pts: %" PRId64 ", dts: %" PRId64 ", cts: %" PRId64 ", diff: %" PRId64 ", bytes: %d\n", pkt->stream->stream, (unsigned int)pkt->stream->codecid, pkt->pts, pkt->dts, pkt->pts - pkt->dts, pkt->dts - s_dts, pkt->size);
	s_dts = pkt->dts;

	r = (int)fwrite(pkt->data, 1, pkt->size, (FILE*)param);
	assert(r == pkt->size);
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

	fp = fopen("rtp.bin", "wb");
	dump = rtpdump_open(file, 0);
#if RTP_DEMUXER
	struct rtp_demuxer_t* demuxer = rtp_demuxer_create(0, 100, 90000, 100, "MP2P", rtp_onpacket, fp);
#else
	struct rtsp_demuxer_t* demuxer = rtsp_demuxer_create(0, 100, rtsp_onpacket, fp);
	r = rtsp_demuxer_add_payload(demuxer, 90000, 99, "H264", "99 packetization-mode=1;profile-level-id=4D4033; sprop-parameter-sets=Z01AM5pkAeACH/4C3AQEBQAAAwPoAAB1MOhgAJ/8AAE/8i7y40MABP/gAAn/kXeXCgA=,aO44gA==");
	assert(0 == r);
#endif
	while (1)
	{
		r = rtpdump_read(dump, &clock, data, sizeof(data));
		if (r <= 0)
			break;

#if RTP_DEMUXER
		r = rtp_demuxer_input(demuxer, data, r);
#else
		r = rtsp_demuxer_input(demuxer, data, r);
#endif
		assert(r >= 0);
	}

#if RTP_DEMUXER
	rtp_demuxer_destroy(&demuxer);
#else
	rtsp_demuxer_destroy(demuxer);
#endif
	rtpdump_close(dump);
	fclose(fp);
}
