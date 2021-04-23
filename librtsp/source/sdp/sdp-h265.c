// RFC7798 RTP Payload Format for High Efficiency Video Coding (HEVC)

#include "mpeg4-hevc.h"
#include "sdp-payload.h"
#include "base64.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int sdp_h265(uint8_t *data, int bytes, const char* proto, unsigned short port, int payload, int frequence, const void* extra, int extra_size)
{
	static const char* pattern =
		"m=video %hu %s %d\n"
		"a=rtpmap:%d H265/90000\n"
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

	for (i = 0; i < sizeof(nalu) / sizeof(nalu[0]); i++)
	{
		if (i > 0 && n < bytes) data[n++] = ';';
		n += snprintf((char*)data + n, bytes - n, " %s=", sprop[i]);

		for (k = j = 0; j < hevc.numOfArrays; j++)
		{
			assert(hevc.nalu[j].type == ((hevc.nalu[j].data[0] >> 1) & 0x3F));
			if (nalu[i] != hevc.nalu[j].type)
				continue;

			if (n + 1 + hevc.nalu[j].bytes * 2 > bytes)
				return -1; // don't have enough memory

			if (k++ > 0 && n < bytes) data[n++] = ',';
			n += base64_encode((char*)data + n, hevc.nalu[j].data, hevc.nalu[j].bytes);
		}
	}

	if(n < bytes) 
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
				return -1; // don't have enough space

			memcpy(data + off, startcode, sizeof(startcode));
			n = (int)base64_decode(data + off + sizeof(startcode), p, len);
			assert(n <= (len + 3) / 4 * 3);
			off += n + sizeof(startcode);
			off += n;

			p = next ? next + 1 : NULL;
		}
	}

	return 0;
}
