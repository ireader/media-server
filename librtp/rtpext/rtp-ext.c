#include "rtp-ext.h"
#include "rtp-header.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static const struct rtp_ext_uri_t sc_rtpexts[] = {
	{RTP_HDREXT_PADDING,						""},

	// https://datatracker.ietf.org/doc/html/rfc6464
	{RTP_HDREXT_SSRC_AUDIO_LEVEL_ID,			"urn:ietf:params:rtp-hdrext:ssrc-audio-level"},
	// https://datatracker.ietf.org/doc/html/rfc6465
	{RTP_HDREXT_CSRC_AUDIO_LEVEL_ID,			"urn:ietf:params:rtp-hdrext:csrc-audio-level"},

	// https://datatracker.ietf.org/doc/html/draft-ietf-avtext-framemarking-13
	//{RTP_HDREXT_FRAME_MARKING_ID,				"http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07"},
	{RTP_HDREXT_FRAME_MARKING_ID,				"urn:ietf:params:rtp-hdrext:framemarking"},

	// https://datatracker.ietf.org/doc/html/rfc8843#section-16.2
	{RTP_HDREXT_SDES_MID_ID,					"urn:ietf:params:rtp-hdrext:sdes:mid"},

	// https://datatracker.ietf.org/doc/html/rfc8852#section-4
	{RTP_HDREXT_SDES_RTP_STREAM_ID,				"urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id"},
	{RTP_HDREXT_SDES_REPAIRED_RTP_STREAM_ID,	"urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id"},

	// https://datatracker.ietf.org/doc/html/rfc5450
	{RTP_HDREXT_TOFFSET_ID,						"urn:ietf:params:rtp-hdrext:toffset"},

	// https://www.arib.or.jp/english/html/overview/doc/STD-T63V12_00/5_Appendix/Rel13/26/26114-d30.pdf
	{RTP_HDREXT_VIDEO_ORIENTATION_ID,			"urn:3gpp:video-orientation"},

	// // https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/abs-send-time
	{RTP_HDREXT_ABSOLUTE_SEND_TIME_ID,			"http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time"},

	// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/abs-capture-time/
	{RTP_HDREXT_ABSOLUTE_CAPTURE_TIME_ID,		"http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time"},

	// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/transport-wide-cc-02/
	{RTP_HDREXT_TRANSPORT_WIDE_CC_ID_01,		"http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01"},
	{RTP_HDREXT_TRANSPORT_WIDE_CC_ID,			"http://www.webrtc.org/experiments/rtp-hdrext/transport-wide-cc-02"},

	// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/video-timing
	{RTP_HDREXT_VIDEO_TIMING_ID,				"http://www.webrtc.org/experiments/rtp-hdrext/video-timing"},

	// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/playout-delay
	{RTP_HDREXT_PLAYOUT_DELAY_ID,				"http://www.webrtc.org/experiments/rtp-hdrext/playout-delay"},

	{RTP_HDREXT_ONE_BYTE_RESERVED,				""},

	// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/color-space
	{RTP_HDREXT_COLOR_SPACE_ID,					"http://www.webrtc.org/experiments/rtp-hdrext/color-space"},

	// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/video-content-type
	{RTP_HDREXT_VIDEO_CONTENT_TYPE_ID,			"http://www.webrtc.org/experiments/rtp-hdrext/video-content-type"},

	// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/inband-cn/
	{RTP_HDREXT_INBAND_CN_ID,					"http://www.webrtc.org/experiments/rtp-hdrext/inband-cn"},

	// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/video-frame-tracking-id/
	{RTP_HDREXT_VIDEO_FRAME_TRACKING_ID,		"http://www.webrtc.org/experiments/rtp-hdrext/video-frame-tracking-id"},

	// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/video-layers-allocation00/
	{RTP_HDREXT_VIDEO_LAYERS_ALLOCATION_ID,		"http://www.webrtc.org/experiments/rtp-hdrext/video-layers-allocation00"},

	//{RTP_HDREXT_GENERIC_FRAME_DESCRIPTOR_00,	"http://www.webrtc.org/experiments/rtp-hdrext/generic-frame-descriptor-00"},
	//{RTP_HDREXT_GENERIC_FRAME_DESCRIPTOR_02,	"http://www.webrtc.org/experiments/rtp-hdrext/generic-frame-descriptor-02"},

	//{RTP_HDREXT_ENCRYPT,						"urn:ietf:params:rtp-hdrext:encrypt"},

	{0, NULL},
};

const struct rtp_ext_uri_t* rtp_ext_list()
{
	return sc_rtpexts;
}

const struct rtp_ext_uri_t* rtp_ext_find_uri(const char* uri)
{
	int i;
	for (i = 0; i < sizeof(sc_rtpexts) / sizeof(sc_rtpexts[0]); i++)
	{
		if (0 == strcmp(sc_rtpexts[i].uri, uri))
			return &sc_rtpexts[i];
	}
	return NULL;
}

static int rtp_ext_read_one_byte(const uint8_t* data, int bytes, struct rtp_ext_data_t exts[256])
{
	int off;
	uint8_t id;
	uint8_t len;

	for (off = 0; off < bytes; off += len + 1)
	{
		id = data[off] >> 4;
		len = (data[off] & 0x0f) + 1;

		if (bytes > 0xFFFF || off + len > bytes || (RTP_HDREXT_PADDING == id && 1 != len))
			return -1; // invalid

		if (RTP_HDREXT_PADDING == data[off])
			continue;
		else if (id == RTP_HDREXT_ONE_BYTE_RESERVED)
			break; // one-byte header extension reserver id

		exts[id].id = id;
		exts[id].len = len;
		exts[id].off = off + 1; // data only
	}

	return 0;
}

static int rtp_ext_write_one_byte(const uint8_t* extension, const struct rtp_ext_data_t* exts, int count, uint8_t* data, int bytes)
{
	int i, off;
	for (i = off = 0; i < count && off < bytes; off += exts[i++].len)
	{
		if (RTP_HDREXT_PADDING == exts[i].id)
		{
			assert(exts[i].len == 0);
			continue;
		}

		// 15: one-byte header extension reserver id
		if (exts[i].id >= RTP_HDREXT_ONE_BYTE_RESERVED 
			|| exts[i].len < 1 || exts[i].len > 16 || (int)exts[i].len + off >= bytes)
		{
			assert(0);
			return -EINVAL;
		}

		data[off] = ((uint8_t)exts[i].id << 4) | ((uint8_t)exts[i].len - 1);
		memcpy(data + off + 1, extension + exts[i].off, exts[i].len);
	}

	if (i < count || ( (off % 4 != 0) && (off + 3)/4*4 >= bytes) )
		return -E2BIG;

	while(off % 4 != 0)
		data[off++] = RTP_HDREXT_PADDING;
	return off;
}

static int rtp_ext_read_two_byte(const uint8_t* data, int bytes, struct rtp_ext_data_t exts[256])
{
	int off;
	uint8_t id;
	uint8_t len;

	for (off = 0; off < bytes; off += len)
	{
		id = data[off++];
		if (RTP_HDREXT_PADDING == id)
		{
			len = 0;
			continue;
		}

		if (off >= bytes)
			return -EINVAL;

		len = data[off++];
		if (off + len > bytes)
			return -EINVAL;; // invalid

		exts[id].id = id;
		exts[id].len = len;
		exts[id].off = off; // data only
	}

	return 0;
}

static int rtp_ext_write_two_byte(const uint8_t* extension, const struct rtp_ext_data_t* exts, int count, uint8_t* data, int bytes)
{
	int i, off;
	for (i = off = 0; i < count && off < bytes; off += exts[i++].len)
	{
		if (RTP_HDREXT_PADDING == exts[i].id)
		{
			assert(exts[i].len == 0);
			continue;
		}

		if ((int)exts[i].len + off + 2 >= bytes)
		{
			assert(0);
			return -EINVAL;
		}

		data[off++] = (uint8_t)exts[i].id;
		data[off++] = (uint8_t)exts[i].len;
		memcpy(data + off, extension + exts[i].off, exts[i].len);
	}

	if (i < count || ((off % 4 != 0) && (off + 3) / 4 * 4 >= bytes))
		return -E2BIG;

	while (off % 4 != 0)
		data[off++] = RTP_HDREXT_PADDING;
	return off;
}

int rtp_ext_read(uint16_t profile, const uint8_t* data, int bytes, struct rtp_ext_data_t exts[256])
{
	// caller to do
	// memset(exts, 0, sizeof(exts));

	if(RTP_HDREXT_PROFILE_ONE_BYTE == profile)
		return rtp_ext_read_one_byte(data, bytes, exts);
	else if (RTP_HDREXT_PROFILE_TWO_BYTE == (profile & RTP_HDREXT_PROFILE_TWO_BYTE_FILTER))
		return rtp_ext_read_two_byte(data, bytes, exts);

	return 0; // ignore
}

int rtp_ext_write(uint16_t profile, const uint8_t* extension, const struct rtp_ext_data_t* exts, int count, uint8_t* data, int bytes)
{
	int i;
	if (0 == profile)
	{
		profile = RTP_HDREXT_PROFILE_ONE_BYTE;
		for (i = 0; i < count; i++)
		{
			// 15: one-byte header extension reserver id
			if (exts[i].len >= 16 || RTP_HDREXT_ONE_BYTE_RESERVED == exts[i].id)
				profile = RTP_HDREXT_PROFILE_TWO_BYTE;
		}
	}

	if (RTP_HDREXT_PROFILE_ONE_BYTE == profile)
		return rtp_ext_write_one_byte(extension, exts, count, data, bytes);
	else if (RTP_HDREXT_PROFILE_TWO_BYTE == (profile & RTP_HDREXT_PROFILE_TWO_BYTE_FILTER))
		return rtp_ext_write_two_byte(extension, exts, count, data, bytes);

	return -1; // ignore
}

#if defined(DEBUG) || defined(_DEBUG)
static void rtp_ext_read_onebyte_test(void)
{
	const uint8_t data[] = { 0x22, 0xca, 0x4e, 0x36, 0x31, 0x00, 0x01, 0x40, 0x30, 0x10, 0xb2, 0x00 };
	struct rtp_ext_data_t exts[256];
	int i;
	memset(exts, 0, sizeof(exts));
	assert(0 == rtp_ext_read(RTP_HDREXT_PROFILE_ONE_BYTE, data, sizeof(data), exts));
	assert(exts[2].len == 3 && exts[3].len == 2 && exts[4].len == 1 && exts[1].len == 1);
	for (i = 0; i < sizeof(exts) / sizeof(exts[0]); i++)
	{
		if (i == 1 || i == 2 || i == 3 || i == 4)
			continue;
		assert(exts[i].id == 0 && exts[i].len == 0);
	}
}

static void rtp_ext_read_twobyte_test(void)
{

}

void rtp_ext_read_test(void)
{
	rtp_ext_read_onebyte_test();
	rtp_ext_read_twobyte_test();
}
#endif
