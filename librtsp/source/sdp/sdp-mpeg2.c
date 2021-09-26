// RFC7587 RTP Payload Format for the Opus Speech and Audio Codec

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "rtp-profile.h"
#include "sdp-payload.h"

int sdp_mpeg2_ps(uint8_t* data, int bytes, const char* proto, unsigned short port, int payload)
{
	static const char* pattern =
		"m=video %hu %s %d\n"
		"a=rtpmap:%d MP2P/90000\n";

	return snprintf((char*)data, bytes, pattern, port, proto && *proto ? proto : "RTP/AVP", payload, payload);
}

int sdp_mpeg2_ts(uint8_t* data, int bytes, const char* proto, unsigned short port)
{
	static const char* pattern =
		"m=video %hu %s 33\n"
		"a=rtpmap:33 MP2T/90000\n";

	return snprintf((char*)data, bytes, pattern, port, proto && *proto ? proto : "RTP/AVP");
}
