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
		sprintf(buf, "%d:%02d:%02d.%03d", (int)(t / 3600000), (int)((t / 60000) % 60), (int)((t / 1000) % 60), (int)(t % 1000));
	}
	return buf;
}

static int on_ts_packet(void* /*param*/, int program, int /*stream*/, int avtype, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	static char s_pts[64], s_dts[64];
    
	if (PSI_STREAM_AAC == avtype)
	{
		static int64_t a_pts = 0, a_dts = 0;
        if (PTS_NO_VALUE == dts)
            dts = pts;
		//assert(0 == a_dts || dts >= a_dts);
		printf("[A] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d\n", ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - a_pts) / 90, (int)(dts - a_dts) / 90);
		a_pts = pts;
		a_dts = dts;

		fwrite(data, 1, bytes, afp);
	}
	else if (PSI_STREAM_H264 == avtype)
	{
		static int64_t v_pts = 0, v_dts = 0;
		assert(0 == v_dts || dts >= v_dts);
		printf("[V] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d%s\n", ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - v_pts) / 90, (int)(dts - v_dts) / 90, flags ? " [I]":"");
		v_pts = pts;
		v_dts = dts;

		fwrite(data, 1, bytes, vfp);
	}
	else
	{
		//assert(0);
	}
    return 0;
}

void mpeg_ts_dec_test(const char* file)
{
	unsigned char ptr[188];
	FILE* fp = fopen(file, "rb");
	vfp = fopen("v.h264", "wb");
	afp = fopen("a.aac", "wb");
    
    ts_demuxer_t *ts = ts_demuxer_create(on_ts_packet, NULL);
	while (1 == fread(ptr, sizeof(ptr), 1, fp))
	{
        ts_demuxer_input(ts, ptr, sizeof(ptr));
	}
    ts_demuxer_flush(ts);
    ts_demuxer_destroy(ts);

	fclose(fp);
	fclose(vfp);
	fclose(afp);
}
