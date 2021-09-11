#include "flv-demuxer.h"
#include "flv-reader.h"
#include "flv-proto.h"
#include <assert.h>
#include <stdio.h>

static unsigned char packet[8 * 1024 * 1024];
static FILE* aac;
static FILE* h264;

inline const char* ftimestamp(uint32_t t, char* buf)
{
	sprintf(buf, "%02u:%02u:%02u.%03u", t / 3600000, (t / 60000) % 60, (t / 1000) % 60, t % 1000);
	return buf;
}

inline size_t get_adts_length(const uint8_t* data, size_t bytes)
{
	assert(bytes >= 6);
	return ((data[3] & 0x03) << 11) | (data[4] << 3) | ((data[5] >> 5) & 0x07);
}

inline char flv_type(int type)
{
	switch (type)
	{
	case FLV_AUDIO_AAC: return 'A';
	case FLV_AUDIO_MP3: return 'M';
	case FLV_AUDIO_ASC: return 'a';
	case FLV_VIDEO_H264: return 'V';
	case FLV_VIDEO_AVCC: return 'v';
	case FLV_VIDEO_H265: return 'H';
	case FLV_VIDEO_HVCC: return 'h';
	default: return '*';
	}
}

static int onFLV(void* /*param*/, int codec, const void* data, size_t bytes, uint32_t pts, uint32_t dts, int flags)
{
	static char s_pts[64], s_dts[64];
	static uint32_t v_pts = 0, v_dts = 0;
	static uint32_t a_pts = 0, a_dts = 0;

	printf("[%c] pts: %s, dts: %s, %u, cts: %d, ", flv_type(codec), ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), dts, (int)(pts - dts));
	
	if (FLV_AUDIO_AAC == codec)
	{
		printf("diff: %03d/%03d", (int)(pts - a_pts), (int)(dts - a_dts));
		a_pts = pts;
		a_dts = dts;

		assert(bytes == get_adts_length((const uint8_t*)data, bytes));
		fwrite(data, bytes, 1, aac);
	}
	else if (FLV_VIDEO_H264 == codec || FLV_VIDEO_H265 == codec)
	{
		printf("diff: %03d/%03d %s", (int)(pts - v_pts), (int)(dts - v_dts), flags ? "[I]" : "");
		v_pts = pts;
		v_dts = dts;

		fwrite(data, bytes, 1, h264);
	}
	else if (FLV_AUDIO_MP3 == codec)
	{
		fwrite(data, bytes, 1, aac);
	}
	else if (FLV_AUDIO_ASC == codec || FLV_VIDEO_AVCC == codec || FLV_VIDEO_HVCC == codec)
	{
		// nothing to do
	}
	else if ((3 << 4) == codec)
	{
		fwrite(data, bytes, 1, aac);
	}
	else
	{
		// nothing to do
		assert(FLV_SCRIPT_METADATA == codec);
	}

	printf("\n");
	return 0;
}

// flv_reader_test("53340.flv");
void flv_reader_test(const char* file)
{
	aac = fopen("audio.aac", "wb");
	h264 = fopen("video.h264", "wb");

	void* reader = flv_reader_create(file);
	flv_demuxer_t* flv = flv_demuxer_create(onFLV, NULL);

	int type, r;
	size_t taglen;
	uint32_t timestamp;
	while (1 == flv_reader_read(reader, &type, &timestamp, &taglen, packet, sizeof(packet)))
	{
		r = flv_demuxer_input(flv, type, packet, taglen, timestamp);
		if (r < 0)
		{
			assert(0);
		}
	}

	flv_demuxer_destroy(flv);
	flv_reader_destroy(reader);

	fclose(aac);
	fclose(h264);
}
