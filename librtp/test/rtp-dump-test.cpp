#include "rtp-dump.h"
#include "rtsp-demuxer.h"
#include <stdio.h>
#include <assert.h>

//static void rtp_onpacket(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
//{
//	int r;
//	r = (int)fwrite(packet, 1, bytes, (FILE*)param);
//	assert(r == bytes);
//}

static int rtsp_onpacket(void* param, int track, int payload, const char* encoding, int64_t pts, int64_t dts, const void* data, int bytes, int flags)
{
	int r;
	r = (int)fwrite(data, 1, bytes, (FILE*)param);
	assert(r == bytes);
	return 0;
}

void rtp_dump_test(const char* file)
{
	int r;
	uint8_t data[1500];
	uint32_t clock;
	struct rtsp_demuxer_t* demuxer;
	struct rtpdump_t* dump;
	FILE* fp;

	fp = fopen("rtp.bin", "wb");
	dump = rtpdump_open(file, 0);
	//demuxer = rtsp_demuxer_create(90000, 96, "MP2P", rtp_onpacket, fp);
	demuxer = rtsp_demuxer_create(90000, 96, "MP2P", NULL, rtsp_onpacket, fp);
	while (1)
	{
		r = rtpdump_read(dump, &clock, data, sizeof(data));
		if (r <= 0)
			break;

		r = rtsp_demuxer_input(demuxer, data, r);
		assert(r >= 0);
	}

	rtsp_demuxer_destroy(demuxer);
	rtpdump_close(dump);
	fclose(fp);
}
