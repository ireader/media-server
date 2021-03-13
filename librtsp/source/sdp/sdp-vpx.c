// RFC7741 RTP Payload Format for VP8 Video
// RTP Payload Format for VP9 Video draft-ietf-payload-vp9-11

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "rtp-profile.h"

int sdp_vp8(uint8_t* data, int bytes, unsigned short port, int payload)
{
	// 6.2.  SDP Parameters
	static const char* pattern =
		"m=video %hu RTP/AVP %d\n"
		"a=rtpmap:%d VP8/90000\n";

	// TODO: AVPF

	return snprintf((char*)data, bytes, pattern, port, payload, payload);
}

int sdp_vp9(uint8_t* data, int bytes, unsigned short port, int payload)
{
	// 6.2.  SDP Parameters
	static const char* pattern =
		"m=video %hu RTP/AVP %d\n"
		"a=rtpmap:%d VP8/90000\n";

	// TODO: AVPF

	return snprintf((char*)data, bytes, pattern, port, payload, payload);
}
