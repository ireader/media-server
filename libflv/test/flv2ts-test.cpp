#include "flv-demuxer.h"
#include "flv-reader.h"
#include "flv-proto.h"
#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <map>

static void* ts_alloc(void* /*param*/, size_t bytes)
{
	static char s_buffer[188];
	assert(bytes <= sizeof(s_buffer));
	return s_buffer;
}

static void ts_free(void* /*param*/, void* /*packet*/)
{
	return;
}

static int ts_write(void* param, const void* packet, size_t bytes)
{
	return 1 == fwrite(packet, bytes, 1, (FILE*)param) ? 0 : ferror((FILE*)param);
}

inline const char* ftimestamp(uint32_t t, char* buf)
{
	sprintf(buf, "%u:%02u:%02u.%03u", t / 3600000, (t / 60000) % 60, (t / 1000) % 60, t % 1000);
	return buf;
}

static inline char flv_type(int type)
{
	switch (type)
	{
	case FLV_AUDIO_ASC:			return 'a';
	case FLV_AUDIO_AAC:			return 'A';
	case FLV_AUDIO_MP3:			return 'M';
	case FLV_AUDIO_OPUS:		return 'O';
	case FLV_AUDIO_OPUS_HEAD:	return 'o';
	case FLV_VIDEO_H264:		return 'V';
	case FLV_VIDEO_AVCC:		return 'v';
	case FLV_VIDEO_H265:		return 'H';
	case FLV_VIDEO_HVCC:		return 'h';
	default: return '*';
	}
}

static inline int flv2ts_codec_id(int type)
{
	switch (type)
	{
	case FLV_AUDIO_ASC:
	case FLV_AUDIO_AAC:			return STREAM_AUDIO_AAC;
	case FLV_AUDIO_MP3:			return STREAM_AUDIO_MP3;
	case FLV_AUDIO_OPUS:		
	case FLV_AUDIO_OPUS_HEAD:	return STREAM_AUDIO_OPUS;
	case FLV_VIDEO_H264:		
	case FLV_VIDEO_AVCC:		return STREAM_VIDEO_H264;
	case FLV_VIDEO_H265:		
	case FLV_VIDEO_HVCC:		return STREAM_VIDEO_H264;
	default: return '*';
	}
}

static int ts_stream(void* ts, int codecid, const void* data, size_t bytes)
{
    static std::map<int, int> streams;
    std::map<int, int>::const_iterator it = streams.find(codecid);
    if (streams.end() != it)
        return it->second;

    int i = mpeg_ts_add_stream(ts, flv2ts_codec_id(codecid), data, bytes);
    streams[codecid] = i;
    return i;
}

static int onFLV(void* ts, int codec, const void* data, size_t bytes, unsigned int pts, unsigned int dts, int flags)
{
	static char s_pts[64], s_dts[64];
	static uint32_t v_pts = 0, v_dts = 0;
	static uint32_t a_pts = 0, a_dts = 0;

	printf("[%c] pts: %s, dts: %s, ", flv_type(codec), ftimestamp(pts, s_pts), ftimestamp(dts, s_dts));

	if (FLV_AUDIO_AAC == codec || FLV_AUDIO_MP3 == codec || FLV_AUDIO_OPUS == codec)
	{
		//		assert(0 == a_dts || dts >= a_dts);
		pts = (a_pts && pts < a_pts) ? a_pts : pts;
		dts = (a_dts && dts < a_dts) ? a_dts : dts;
		mpeg_ts_write(ts, ts_stream(ts, codec, NULL, 0), 0, pts * 90, dts * 90, data, bytes);

		printf("diff: %03d/%03d", (int)(pts - a_pts), (int)(dts - a_dts));
		a_pts = pts;
		a_dts = dts;
	}
	else if (FLV_VIDEO_H264 == codec || FLV_VIDEO_H265 == codec)
	{
		assert(0 == v_dts || dts >= v_dts);
		dts = (a_dts && dts < v_dts) ? v_dts : dts;
		mpeg_ts_write(ts, ts_stream(ts, codec, NULL, 0), 0x01 & flags ? 1 : 0, pts * 90, dts * 90, data, bytes);

		printf("diff: %03d/%03d%s", (int)(pts - v_pts), (int)(dts - v_dts), flags ? " [I]" : "");
		v_pts = pts;
		v_dts = dts;
	}
	else if (FLV_AUDIO_OPUS_HEAD == codec)
	{
		ts_stream(ts, FLV_AUDIO_OPUS, data, bytes);
	}
	else
	{
		// nothing to do
	}

	printf("\n");
	return 0;
}

void flv2ts_test(const char* inputFLV, const char* outputTS)
{
	struct mpeg_ts_func_t tshandler;
	tshandler.alloc = ts_alloc;
	tshandler.write = ts_write;
	tshandler.free = ts_free;

	FILE* fp = fopen(outputTS, "wb");
	void* ts = mpeg_ts_create(&tshandler, fp);
	void* reader = flv_reader_create(inputFLV);
	flv_demuxer_t* flv = flv_demuxer_create(onFLV, ts);

	int type, r;
	size_t taglen;
	uint32_t timestamp;
	static unsigned char s_packet[8 * 1024 * 1024];
	while (1 == flv_reader_read(reader, &type, &timestamp, &taglen, s_packet, sizeof(s_packet)))
	{
		r = flv_demuxer_input(flv, type, s_packet, taglen, timestamp);
		if (r < 0)
		{
			assert(0);
		}
	}

	flv_demuxer_destroy(flv);
	flv_reader_destroy(reader);
	mpeg_ts_destroy(ts);
	fclose(fp);
}
