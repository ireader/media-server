// RFC6184 RTP Payload Format for H.264 Video

#include "mpeg4-avc.h"
#include "sdp-payload.h"
#include "base64.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int sdp_h264(uint8_t *data, int bytes, const char* proto, unsigned short port, int payload, int frequence, const void* extra, int extra_size)
{
	static const char* pattern =
		"m=video %hu %s %d\n"
		"a=rtpmap:%d H264/90000\n"
		"a=fmtp:%d packetization-mode=1;profile-level-id=%02X%02X%02X;sprop-parameter-sets=";

	int r, n;
	uint8_t i;
	struct mpeg4_avc_t avc;

	assert(90000 == frequence);
	r = mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, extra_size, &avc);
	if (r < 0) return r;
	assert(avc.nb_pps + avc.nb_sps > 0);

	n = snprintf((char*)data, bytes, pattern, port, proto && *proto ? proto : "RTP/AVP", payload, payload, payload,
		(unsigned int)avc.profile, (unsigned int)avc.compatibility, (unsigned int)avc.level);

	for (i = 0; i < avc.nb_sps; i++)
	{
		if (n + 1 + avc.sps[i].bytes * 2 > bytes)
			return -1; // // don't have enough memory

		if (i > 0 && n < bytes) data[n++] = ',';
		n += base64_encode((char*)data + n, avc.sps[i].data, avc.sps[i].bytes);
	}

	for (i = 0; i < avc.nb_pps; i++)
	{
		if (n + 1 + avc.sps[i].bytes * 2 > bytes)
			return -1; // // don't have enough memory

		if (n < bytes) data[n++] = ',';
		n += base64_encode((char*)data + n, avc.pps[i].data, avc.pps[i].bytes);
	}

	if (n < bytes)
		data[n++] = '\n';
	return n;
}

int sdp_h264_load(uint8_t* data, int bytes, const char* config)
{
	int n, len, off;
	const char* p, *next;
	const uint8_t startcode[] = { 0x00, 0x00, 0x00, 0x01 };

	off = 0;
	p = config;
	while (p)
	{
		next = strchr(p, ',');
		len = next ? (int)(next - p) : (int)strlen(p);
		if (off + (len + 3) / 4 * 3 + (int)sizeof(startcode) > bytes)
			return -1; // don't have enough space

		memcpy(data + off, startcode, sizeof(startcode));
		n = (int)base64_decode(data + off + sizeof(startcode), p, len);
		assert(n <= (len + 3) / 4 * 3);
		off += n + sizeof(startcode);

		p = next ? next + 1 : NULL;
	}

	return off;
}

#if defined(_DEBUG) || defined(DEBUG)
void sdp_h264_test(void)
{
	const char* sdp = "m=video 0 RTP/AVP 96\na=rtpmap:96 H264/90000\na=fmtp:96 packetization-mode=1;profile-level-id=64001F;sprop-parameter-sets=Z2QAH6zZQFAFumoCGgKAAAADAIAAAB5HjBjL,aO+8sA==\n";
	const char* config = "Z2QAH6zZQFAFumoCGgKAAAADAIAAAB5HjBjL,aO+8sA==";
	static const uint8_t extra[] = { 0x01, 0x64, 0x00, 0x1f, 0xff, 0xe1, 0x00, 0x1b, 0x67, 0x64, 0x00, 0x1f, 0xac, 0xd9, 0x40, 0x50, 0x05, 0xba, 0x6a, 0x02, 0x1a, 0x02, 0x80, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x1e, 0x47, 0x8c, 0x18, 0xcb, 0x01, 0x00, 0x04, 0x68, 0xef, 0xbc, 0xb0 };
	static const uint8_t ps[] = { 0x00, 0x00, 0x00, 0x1, 0x67, 0x64, 0x00, 0x1f, 0xac, 0xd9, 0x40, 0x50, 0x05, 0xba, 0x6a, 0x02, 0x1a, 0x02, 0x80, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x1e, 0x47, 0x8c, 0x18, 0xcb, 0x00, 0x00, 0x00, 0x1, 0x68, 0xef, 0xbc, 0xb0 };
	uint8_t buffer[256];

	assert((int)strlen(sdp) == sdp_h264(buffer, sizeof(buffer), "RTP/AVP", 0, 96, 90000, extra, sizeof(extra)));
	assert(0 == memcmp(sdp, buffer, strlen(sdp)));

	assert(sizeof(ps) == sdp_h264_load(buffer, sizeof(buffer), config));
	assert(0 == memcmp(buffer, ps, sizeof(ps)));
}
#endif
