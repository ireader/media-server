#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include <stdio.h>

static void ts_packet(void* param, int avtype, int64_t pts, int64_t dts, void* data, size_t bytes)
{
	printf("[%d] pts: %lld, dts: %lld\n", avtype, pts, dts);
}

void mpeg_ts_dec_test(const char* file)
{
	unsigned char ptr[188];
	FILE* fp = fopen(file, "rb");
	while (1 == fread(ptr, sizeof(ptr), 1, fp))
	{
		mpeg_ts_packet_dec(ptr, sizeof(ptr), ts_packet, NULL);
	}
	fclose(fp);
}
