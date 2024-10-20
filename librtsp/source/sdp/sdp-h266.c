// RFC 9328 RTP Payload Format for Versatile Video Coding(VVC)

#include "mpeg4-vvc.h"
#include "sdp-payload.h"
#include "base64.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

int sdp_h266(uint8_t *data, int bytes, const char* proto, unsigned short port, int payload, int frequence, const void* extra, int extra_size)
{
	static const char* pattern =
		"m=video %hu %s %d\n"
		"a=rtpmap:%d H266/90000\n"
		"a=fmtp:%d";

	const uint8_t nalu[] = { 13/*dci*/, 14/*vps*/, 15/*sps*/, 16/*pps*/ };
	const char* sprop[] = { "sprop-dci", "sprop-vps", "sprop-sps", "sprop-pps" };

	int r, n;
	int i, j, k;
	struct mpeg4_vvc_t vvc;

	assert(90000 == frequence);
	r = mpeg4_vvc_decoder_configuration_record_load((const uint8_t*)extra, extra_size, &vvc);
	if (r < 0) 
		return r;

	n = snprintf((char*)data, bytes, pattern, port, proto && *proto ? proto : "RTP/AVP", payload, payload, payload);

	for (i = 0; i < sizeof(nalu) / sizeof(nalu[0]) && n < bytes; i++)
	{
		if (i > 0 && n < bytes) data[n++] = ';';
		n += snprintf((char*)data + n, bytes - n, " %s=", sprop[i]);

		for (k = j = 0; j < vvc.numOfArrays; j++)
		{
			assert(vvc.nalu[j].bytes > 2 && vvc.nalu[j].type == ((vvc.nalu[j].data[1] >> 3) & 0x1F));
			if (nalu[i] != vvc.nalu[j].type)
				continue;

			if (n + 1 + vvc.nalu[j].bytes * 2 > bytes)
				return -ENOMEM; // don't have enough memory

			if (k++ > 0 && n < bytes) data[n++] = ',';
			n += (int)base64_encode((char*)data + n, vvc.nalu[j].data, vvc.nalu[j].bytes);
		}
	}

	if(n < bytes) 
		data[n++] = '\n';
	return n;
}

int sdp_h266_load(uint8_t* data, int bytes, const char* vps, const char* sps, const char* pps, const char* sei, const char* dci)
{
	int i, n, len, off;
	const char* p, * next;
	const char* sprops[5];
	const uint8_t startcode[] = { 0x00, 0x00, 0x00, 0x01 };

	off = 0;
	sprops[0] = vps;
	sprops[1] = sps;
	sprops[2] = pps;
	sprops[3] = sei;
	sprops[4] = dci;
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
			off += n;

			p = next ? next + 1 : NULL;
		}
	}

	return 0;
}
