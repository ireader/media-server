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
static int rtsp_onpacket(void* param, int track, int payload, const char* encoding, int64_t pts, int64_t dts, const void* data, int bytes, int flags)
{
	int r;

	static int64_t s_dts = 0;
	if (0 == s_dts)
		s_dts = dts;
	printf("[%d:%s] pts: %" PRId64 ", dts: %" PRId64 ", cts: %" PRId64 ", diff: %" PRId64 ", bytes: %d\n", track, encoding, pts, dts, pts - dts, dts - s_dts, bytes);
	s_dts = dts;

	r = (int)fwrite(data, 1, bytes, (FILE*)param);
	assert(r == bytes);
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
	struct rtp_demuxer_t* demuxer = rtp_demuxer_create(100, 90000, 100, "MP2P", rtp_onpacket, fp);
#else
	struct rtsp_demuxer_t* demuxer = rtsp_demuxer_create(100, 90000, 96, "H264", NULL, rtsp_onpacket, fp);
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
