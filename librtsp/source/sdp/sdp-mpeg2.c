// RFC7587 RTP Payload Format for the Opus Speech and Audio Codec

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "rtp-profile.h"

int sdp_mpeg2_ps(uint8_t* data, int bytes, unsigned short port, int payload)
{
	static const char* pattern =
		"m=video %hu RTP/AVP %d\n"
		"a=rtpmap:%d MP2P/90000\n";

	return snprintf((char*)data, bytes, pattern, port, payload, payload);
}

int sdp_mpeg2_ts(uint8_t* data, int bytes, unsigned short port)
{
	static const char* pattern =
		"m=video %hu RTP/AVP 33\n"
		"a=rtpmap:33 MP2T/90000\n";

	return snprintf((char*)data, bytes, pattern, port);
}
