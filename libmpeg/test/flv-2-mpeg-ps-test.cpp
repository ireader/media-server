#include "flv-demuxer.h"
#include "flv-reader.h"
#include "flv-proto.h"
#include "mpeg-ps-proto.h"
#include "mpeg-ps.h"
#include "sys/system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <map>

static void* ps_alloc(void* /*param*/, size_t bytes)
{
	static char s_buffer[2 * 1024 * 1024];
	assert(bytes <= sizeof(s_buffer));
	return s_buffer;
}

static void ps_free(void* /*param*/, void* /*packet*/)
{
	return;
}

static void ps_write(void* param, int stream, void* packet, size_t bytes)
{
	fwrite(packet, bytes, 1, (FILE*)param);
}

static inline const char* ps_type(int type)
{
	switch (type)
	{
	case FLV_AUDIO_MP3: return "MP3";
	case FLV_AUDIO_AAC: return "AAC";
	case FLV_VIDEO_H264: return "H264";
	case FLV_VIDEO_H265: return "H265";
	default: return "*";
	}
}

static int flv_ondemux(void* ps, int codec, const void* data, size_t bytes, uint32_t pts, uint32_t dts, int flags)
{
	printf("[%s] pts: %08lu, dts: %08lu%s\n", ps_type(codec), (unsigned long)pts, (unsigned long)dts, flags ? " [I]" : "");

	int i;
	static std::map<int, int> streams;
	std::map<int, int>::const_iterator it = streams.find(codec);
	if (streams.end() == it)
	{
		switch (codec)
		{
		case FLV_AUDIO_MP3:
			i = ps_muxer_add_stream((ps_muxer_t*)ps, STREAM_AUDIO_MP3, NULL, 0);
			break;
		case FLV_AUDIO_ASC:
			i = ps_muxer_add_stream((ps_muxer_t*)ps, STREAM_AUDIO_AAC, NULL, 0);
			streams[FLV_AUDIO_AAC] = i;
			return 0;
		case FLV_VIDEO_AVCC:
			i = ps_muxer_add_stream((ps_muxer_t*)ps, STREAM_VIDEO_H264, NULL, 0);
			streams[FLV_VIDEO_H264] = i;
			return 0;
		case FLV_VIDEO_HVCC:
			i = ps_muxer_add_stream((ps_muxer_t*)ps, STREAM_VIDEO_H265, NULL, 0);
			streams[FLV_VIDEO_H265] = i;
			return 0;
		default: return 0;
		}
		streams[codec] = i;
	}
	else
	{
		i = it->second;
	}

	return ps_muxer_input((ps_muxer_t*)ps, i, flags ? 0x01 : 0x00, pts * 90, dts * 90, data, bytes);
}

void flv_2_mpeg_ps_test(const char* flv)
{
	char output[256] = { 0 };
	snprintf(output, sizeof(output), "%s.ps", flv);

	struct ps_muxer_func_t handler;
	handler.alloc = ps_alloc;
	handler.write = ps_write;
	handler.free = ps_free;

	FILE* fp = fopen(output, "wb");
	ps_muxer_t* ps = ps_muxer_create(&handler, fp);

	void* f = flv_reader_create(flv);
	flv_demuxer_t* demuxer = flv_demuxer_create(flv_ondemux, ps);

	int r, type;
	uint32_t timestamp;
	static uint8_t packet[2 * 1024 * 1024];
	while ((r = flv_reader_read(f, &type, &timestamp, packet, sizeof(packet))) > 0)
	{
		assert(0 == flv_demuxer_input(demuxer, type, packet, r, timestamp));
	}

	flv_demuxer_destroy(demuxer);
	flv_reader_destroy(f);
	ps_muxer_destroy(ps);
	fclose(fp);
}
