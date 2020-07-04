#include "rtp-dump.h"
#include "rtp-demuxer.h"
#include <stdio.h>
#include <assert.h>

static void rtp_onpacket(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
	int r;
	r = (int)fwrite(packet, 1, bytes, (FILE*)param);
	assert(r == bytes);
}

void rtp_dump_test(const char* file)
{
	int r;
	uint8_t data[1500];
	uint32_t clock;
	struct rtp_demuxer_t* demuxer;
	struct rtpdump_t* dump;
	FILE* fp;

	fp = fopen("rtp.bin", "wb");
	dump = rtpdump_open(file, 0);
	demuxer = rtp_demuxer_create(90000, 97, "MP2P", rtp_onpacket, fp);
	while (1)
	{
		r = rtpdump_read(dump, &clock, data, sizeof(data));
		if (r <= 0)
			break;

		r = rtp_demuxer_input(demuxer, data, r);
		assert(r >= 0);
	}

	rtp_demuxer_destroy(&demuxer);
	rtpdump_close(dump);
	fclose(fp);
}
