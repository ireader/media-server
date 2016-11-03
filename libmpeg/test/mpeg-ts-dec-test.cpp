#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
#include <stdio.h>
#include <assert.h>

static FILE* vfp;
static FILE* afp;

inline const char* ftimestamp(int64_t t, char* buf)
{
	if (PTS_NO_VALUE == t)
	{
		sprintf(buf, "(null)");
	}
	else
	{
		t /= 90;
		sprintf(buf, "%d:%02d:%02d.%03d", (int)(t / 36000000), (int)((t / 60000) % 60), (int)((t / 1000) % 60), (int)(t % 1000));
	}
	return buf;
}

static void ts_packet(void* /*param*/, int avtype, int64_t pts, int64_t dts, void* data, size_t bytes)
{
	static char s_pts[64], s_dts[64];

	if (PSI_STREAM_AAC == avtype)
	{
		static int64_t a_pts = 0, a_dts = 0;
		assert(0 == a_dts || dts >= a_dts);
		printf("[A] pts: %s, dts: %s, diff: %03d/%03d\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - a_pts) / 90, (int)(dts - a_dts) / 90);
		a_pts = pts;
		a_dts = dts;

		fwrite(data, 1, bytes, afp);
	}
	else if (PSI_STREAM_H264 == avtype)
	{
		static int64_t v_pts = 0, v_dts = 0;
		assert(0 == v_dts || dts >= v_dts);
		printf("[V] pts: %s, dts: %s, diff: %03d/%03d\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - v_pts) / 90, (int)(dts - v_dts) / 90);
		v_pts = pts;
		v_dts = dts;

		fwrite(data, 1, bytes, vfp);
	}
	else
	{
		assert(0);
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
