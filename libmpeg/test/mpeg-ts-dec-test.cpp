#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
#include <stdio.h>

static FILE* vfp;
static FILE* afp;

static void ts_packet(void* param, int avtype, int64_t pts, int64_t dts, void* data, size_t bytes)
{
	printf("[%d] pts: %lld, dts: %lld\n", avtype, pts, dts);

	if (PSI_STREAM_AAC == avtype)
	{
		fwrite(data, 1, bytes, afp);
	}
	else if (PSI_STREAM_H264 == avtype)
	{
		fwrite(data, 1, bytes, vfp);
	}
}

void mpeg_ts_dec_test(const char* file)
{
	unsigned char ptr[188];
	FILE* fp = fopen(file, "rb");
	vfp = fopen("v.h264", "wb");
	afp = fopen("a.aac", "wb");

	while (1 == fread(ptr, sizeof(ptr), 1, fp))
	{
		mpeg_ts_packet_dec(ptr, sizeof(ptr), ts_packet, NULL);
	}
	fclose(fp);
	fclose(vfp);
	fclose(afp);
}
