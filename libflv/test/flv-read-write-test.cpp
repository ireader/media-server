#include "flv-reader.h"
#include "flv-writer.h"
#include "flv-demuxer.h"
#include "flv-muxer.h"
#include "flv-proto.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static int flv_onmuxer(void* flv, int type, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, type, data, bytes, timestamp);
}

static int flv_ondemuxer(void* flv, int codec, const void* data, size_t bytes, uint32_t pts, uint32_t dts, int format)
{
	switch (codec)
	{
	case FLV_AUDIO_AAC:
		flv_muxer_aac(flv, data, bytes, pts, dts);
		break;

	case FLV_AUDIO_MP3:
		flv_muxer_mp3(flv, data, bytes, pts, dts);
		break;

	case FLV_VIDEO_H264:
		flv_muxer_avc(flv, data, bytes, pts, dts);
		break;

	default:
		break;
	}
	return 0;
}

void flv_read_write_test(const char* flv)
{
	static char packet[2 * 1024 * 1024];
	snprintf(packet, sizeof(packet), "%s.flv", flv);

	void* r = flv_reader_create(flv);
	void* w = flv_writer_create(packet);
	void* e = flv_muxer_create(flv_onmuxer, w);
	void* d = flv_demuxer_create(flv_ondemuxer, e);

	int ret, tag;
	uint32_t timestamp;
	while ((ret = flv_reader_read(r, &tag, &timestamp, packet, sizeof(packet))) > 0)
	{
		flv_demuxer_input(d, tag, packet, ret, timestamp);
	}

	flv_muxer_destroy(e);
	flv_demuxer_destroy(d);
	flv_reader_destroy(r);
	flv_writer_destroy(w);
}
