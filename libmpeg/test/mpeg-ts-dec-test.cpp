#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "mpeg-types.h"
#include "mpeg4-aac.h"
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

static int on_ts_packet(void* /*param*/, int program, int stream, int avtype, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	static char s_pts[64], s_dts[64];
    
	if (PSI_STREAM_AAC == avtype || PSI_STREAM_AUDIO_OPUS == avtype)
	{
		static int64_t a_pts = 0, a_dts = 0;
        if (PTS_NO_VALUE == dts)
            dts = pts;
		//assert(0 == a_dts || dts >= a_dts);
		printf("[A][%d:%d] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d, bytes: %u%s%s\n", program, stream, ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - a_pts) / 90, (int)(dts - a_dts) / 90, (unsigned int)bytes, flags & MPEG_FLAG_PACKET_CORRUPT ? " [X]" : "", flags & MPEG_FLAG_PACKET_LOST ? " [-]" : "");
		a_pts = pts;
		a_dts = dts;

		fwrite(data, 1, bytes, afp);

		int count = 0;
		int len = mpeg4_aac_adts_frame_length((const uint8_t*)data, bytes);
		while (len > 7 && (size_t)len <= bytes)
		{
			count++;
			bytes -= len;
			data = (const uint8_t*)data + len;
			len = mpeg4_aac_adts_frame_length((const uint8_t*)data, bytes);
		}
	}
	else if (PSI_STREAM_H264 == avtype || PSI_STREAM_H265 == avtype || PSI_STREAM_H266 == avtype || PSI_STREAM_VIDEO_AVS3 == avtype)
	{
		static int64_t v_pts = 0, v_dts = 0;
		//assert(0 == v_dts || dts >= v_dts);
		printf("[V][%d:%d] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d, bytes: %u%s%s%s\n", program, stream, ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - v_pts) / 90, (int)(dts - v_dts) / 90, (unsigned int)bytes, flags & MPEG_FLAG_IDR_FRAME ? " [I]":"", flags & MPEG_FLAG_PACKET_CORRUPT ? " [X]" : "", flags & MPEG_FLAG_PACKET_LOST ? " [-]" : "");
		v_pts = pts;
		v_dts = dts;

		fwrite(data, 1, bytes, vfp);
	}
	else
	{
		static int64_t x_pts = 0, x_dts = 0;
		//assert(0 == x_dts || dts >= x_dts);
		printf("[%d][%d:%d] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d%s%s%s\n", avtype, program, stream, ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - x_pts) / 90, (int)(dts - x_dts) / 90, flags & MPEG_FLAG_IDR_FRAME ? " [I]" : "", flags & MPEG_FLAG_PACKET_CORRUPT ? " [X]" : "", flags & MPEG_FLAG_PACKET_LOST ? " [-]" : "");
		x_pts = pts;
		x_dts = dts;
		//assert(0);
	}
    return 0;
}

static void mpeg_ts_dec_testonstream(void* param, int stream, int codecid, const void* extra, int bytes, int finish)
{
	printf("stream %d, codecid: %d, finish: %s\n", stream, codecid, finish ? "true" : "false");
}

void mpeg_ts_dec_test(const char* file)
{
	unsigned char ptr[188];
	FILE* fp = fopen(file, "rb");
	vfp = fopen("v.h264", "wb");
	afp = fopen("a.aac", "wb");

	struct ts_demuxer_notify_t notify = {
		mpeg_ts_dec_testonstream,
	};

    ts_demuxer_t *ts = ts_demuxer_create(on_ts_packet, NULL);
	ts_demuxer_set_notify(ts, &notify, NULL);
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
