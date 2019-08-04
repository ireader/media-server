// RFC6184 RTP Payload Format for H.264 Video

#include "mpeg4-avc.h"
#include "base64.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int sdp_h264(uint8_t *data, int bytes, unsigned short port, int payload, int frequence, const void* extra, int extra_size)
{
	static const char* pattern =
		"m=video %hu RTP/AVP %d\n"
		"a=rtpmap:%d H264/90000\n"
		"a=fmtp:%d profile-level-id=%02X%02X%02X;packetization-mode=1;sprop-parameter-sets=";

	int r, n;
	uint8_t i;
	struct mpeg4_avc_t avc;

	r = mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, extra_size, &avc);
	if (r < 0) return r;
	assert(avc.nb_pps + avc.nb_sps > 0);

	n = snprintf((char*)data, bytes, pattern, port, payload, payload, payload,
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
