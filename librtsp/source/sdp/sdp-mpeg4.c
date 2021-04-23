// RFC3640 RTP Payload Format for Transport of MPEG-4 Elementary Streams
// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams

#include "mpeg4-avc.h"
#include "sdp-payload.h"
#include "base64.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int sdp_mpeg4_es(uint8_t *data, int bytes, const char* proto, unsigned short port, int payload, int frequence, const void* extra, int extra_size)
{
	static const char* pattern =
		"m=video %hu %s %d\n"
		"a=rtpmap:%d MP4V-ES/90000\n"
		"a=fmtp:%d profile-level-id=1;config=";

	int n;

	assert(90000 == frequence);
	n = snprintf((char*)data, bytes, pattern, port, proto && *proto ? proto : "RTP/AVP", payload, payload, payload);

	if (n + extra_size * 2 + 1 > bytes)
		return -1; // // don't have enough memory

	// It is a hexadecimal representation of an octet string that 
	// expresses the MPEG-4 Visual configuration information
	n += base16_encode((char*)data + n, extra, extra_size);

	if (n < bytes)
		data[n++] = '\n';
	return n;
}
