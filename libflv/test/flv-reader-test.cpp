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
	sprintf(buf, "%02u:%02u:%02u.%03u", t / 36000000, (t / 60000) % 60, (t / 1000) % 60, t % 1000);
	return buf;
}

inline size_t get_astd_length(const uint8_t* data, size_t bytes)
{
	assert(bytes >= 6);
	return ((data[3] & 0x03) << 11) | (data[4] << 3) | ((data[5] >> 5) & 0x07);
}

inline char flv_type(int type, int format)
{
	format = (FLV_TYPE_AUDIO == type) ? (format << 8) : format;

	switch (format)
	{
	case (FLV_AUDIO_AAC << 8): return 'A';
	case (FLV_AUDIO_MP3 << 8): return 'M';
	case (FLV_AUDIO_ASC << 8): return 'a';
	case FLV_VIDEO_H264: return 'V';
	case FLV_VIDEO_AVCC: return 'v';
	default: return '*';
	}
}

static void onFLV(void* /*param*/, int type, int format, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	static char s_pts[64], s_dts[64];
	static uint32_t v_pts = 0, v_dts = 0;
	static uint32_t a_pts = 0, a_dts = 0;

	printf("[%c] pts: %s, dts: %s, ", flv_type(type, format), ftimestamp(pts, s_pts), ftimestamp(dts, s_dts));

	format = (FLV_TYPE_AUDIO == type) ? (format << 8) : format;
	if ((FLV_AUDIO_AAC << 8) == format)
	{
		printf("diff: %03d/%03d", (int)(pts - a_pts), (int)(dts - a_dts));
		a_pts = pts;
		a_dts = dts;

		assert(bytes == get_astd_length((const uint8_t*)data, bytes));
		fwrite(data, bytes, 1, aac);
	}
	else if (FLV_VIDEO_H264 == format)
	{
		printf("diff: %03d/%03d", (int)(pts - v_pts), (int)(dts - v_dts));
		v_pts = pts;
		v_dts = dts;

		fwrite(data, bytes, 1, h264);
	}
	else if ((FLV_AUDIO_MP3 << 8) == format)
	{
		fwrite(data, bytes, 1, aac);
	}
	else if ((FLV_AUDIO_ASC << 8) == format || FLV_VIDEO_AVCC == format)
	{
		// nothing to do
	}
	else
	{
		// nothing to do
		assert(0);
	}

	printf("\n");
}

// flv_reader_test("53340.flv");
void flv_reader_test(const char* file)
{
	aac = fopen("audio.aac", "wb");
	h264 = fopen("video.h264", "wb");

	void* reader = flv_reader_create(file);
	void* flv = flv_demuxer_create(onFLV, NULL);

	int type, r = 0;
	uint32_t timestamp;
	while ((r = flv_reader_read(reader, &type, &timestamp, packet, sizeof(packet))) > 0)
	{
		r = flv_demuxer_input(flv, type, packet, r, timestamp);
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
