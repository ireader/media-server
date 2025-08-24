// RFC7798 RTP Payload Format for High Efficiency Video Coding (HEVC)

#include "mpeg4-hevc.h"
#include "sdp-payload.h"
#include "base64.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

int sdp_h265(uint8_t *data, int bytes, const char* proto, unsigned short port, int payload, int frequence, const void* extra, int extra_size)
{
	static const char* pattern =
		"m=video %hu %s %d\r\n"
		"a=rtpmap:%d H265/90000\r\n"
		"a=fmtp:%d";

	const uint8_t nalu[] = { 32/*vps*/, 33/*sps*/, 34/*pps*/ };
	const char* sprop[] = { "sprop-vps", "sprop-sps", "sprop-pps" };

	int r, n;
	int i, j, k;
	struct mpeg4_hevc_t hevc;

	assert(90000 == frequence);
	r = mpeg4_hevc_decoder_configuration_record_load((const uint8_t*)extra, extra_size, &hevc);
	if (r < 0) 
		return r;

	n = snprintf((char*)data, bytes, pattern, port, proto && *proto ? proto : "RTP/AVP", payload, payload, payload);

	for (i = 0; i < sizeof(nalu) / sizeof(nalu[0]) && n < bytes; i++)
	{
		if (i > 0 && n < bytes) data[n++] = ';';
		n += snprintf((char*)data + n, bytes - n, " %s=", sprop[i]);

		for (k = j = 0; j < hevc.numOfArrays; j++)
		{
			assert(hevc.nalu[j].type == ((hevc.nalu[j].data[0] >> 1) & 0x3F));
			if (nalu[i] != hevc.nalu[j].type)
				continue;

			if (n + 1 + hevc.nalu[j].bytes * 2 > bytes)
				return -ENOMEM; // don't have enough memory

			if (k++ > 0 && n < bytes) data[n++] = ',';
			n += (int)base64_encode((char*)data + n, hevc.nalu[j].data, hevc.nalu[j].bytes);
		}
	}

	if (n + 2 > bytes)
		return -ENOMEM; // don't have enough memory

	data[n++] = '\r';
	data[n++] = '\n';
	return n;
}

int sdp_h265_load(uint8_t* data, int bytes, const char* vps, const char* sps, const char* pps, const char* sei)
{
	int i, n, len, off;
	const char* p, * next;
	const char* sprops[4];
	const uint8_t startcode[] = { 0x00, 0x00, 0x00, 0x01 };

	off = 0;
	sprops[0] = vps;
	sprops[1] = sps;
	sprops[2] = pps;
	sprops[3] = sei;
	for (i = 0; i < sizeof(sprops) / sizeof(sprops[0]); i++)
	{
		p = sprops[i];
		while (p)
		{
			next = strchr(p, ',');
			len = next ? (int)(next - p) : (int)strlen(p);
			if (off + (len + 3) / 4 * 3 + (int)sizeof(startcode) > bytes)
				return -ENOMEM; // don't have enough space

			memcpy(data + off, startcode, sizeof(startcode));
			n = (int)base64_decode(data + off + sizeof(startcode), p, len);
			assert(n <= (len + 3) / 4 * 3);
			off += n + sizeof(startcode);

			p = next ? next + 1 : NULL;
		}
	}

	return off;
}

#if defined(_DEBUG) || defined(DEBUG)
void sdp_h265_test(void)
{
	const char* sdp = "m=video 0 RTP/AVP 96\r\na=rtpmap:96 H265/90000\r\na=fmtp:96 sprop-vps=QAEMAf//AWAAAAMAsAAAAwAAAwBdFcCQ; sprop-sps=QgEBAWAAAAMAsAAAAwAAAwBdoAWiAFAWIFe5FlRA; sprop-pps=RAHALLwUyQ==\r\n";
	static const uint8_t extra[] = { 0x01, 0x01, 0x60, 0x00, 0x00, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5d, 0xf0, 0x00, 0xfc, 0xfd, 0xf8, 0xf8, 0x00, 0x00, 0x07, 0x03, 0x20, 0x00, 0x01, 0x00, 0x18, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0xb0, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x5d, 0x15, 0xc0, 0x90, 0x21, 0x00, 0x01, 0x00, 0x1e, 0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0xb0, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x5d, 0xa0, 0x05, 0xa2, 0x00, 0x50, 0x16, 0x20, 0x57, 0xb9, 0x16, 0x54, 0x40, 0x22, 0x00, 0x01, 0x00, 0x07, 0x44, 0x01, 0xc0, 0x2c, 0xbc, 0x14, 0xc9 };
	static const uint8_t ps[] = { 0x00, 0x00, 0x00, 0x1, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0xb0, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x5d, 0x15, 0xc0, 0x90, 0x00, 0x00, 0x00, 0x1, 0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0xb0, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x5d, 0xa0, 0x05, 0xa2, 0x00, 0x50, 0x16, 0x20, 0x57, 0xb9, 0x16, 0x54, 0x40, 0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xc0, 0x2c, 0xbc, 0x14, 0xc9 };
	uint8_t buffer[256], vps[16], sps[64], pps[64], sei[64];

	assert((int)strlen(sdp) == sdp_h265(buffer, sizeof(buffer), "RTP/AVP", 0, 96, 90000, extra, sizeof(extra)));
	assert(0 == memcmp(sdp, buffer, strlen(sdp)));

	assert(sizeof(ps) == sdp_h265_load(buffer, sizeof(buffer), "QAEMAf//AWAAAAMAsAAAAwAAAwBdFcCQ", "QgEBAWAAAAMAsAAAAwAAAwBdoAWiAFAWIFe5FlRA", "RAHALLwUyQ==", NULL));
	assert(0 == memcmp(buffer, ps, sizeof(ps)));
}
#endif
