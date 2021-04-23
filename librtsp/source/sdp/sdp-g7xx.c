// RFC7587 RTP Payload Format for the Opus Speech and Audio Codec

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "sdp-payload.h"

int sdp_g711u(uint8_t *data, int bytes, const char* proto, unsigned short port)
{
	static const char* pattern = "m=audio %hu %s 0\n";
	return snprintf((char*)data, bytes, pattern, port, proto && *proto ? proto : "RTP/AVP");
}

int sdp_g711a(uint8_t *data, int bytes, const char* proto, unsigned short port)
{
	static const char* pattern = "m=audio %hu %s 8\n";
	return snprintf((char*)data, bytes, pattern, port, proto && *proto ? proto : "RTP/AVP");
}
