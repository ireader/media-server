// RFC7587 RTP Payload Format for the Opus Speech and Audio Codec

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int sdp_g711u(uint8_t *data, int bytes, unsigned short port)
{
	static const char* pattern = "m=audio %hu RTP/AVP 0\n";
	return snprintf((char*)data, bytes, pattern, port);
}

int sdp_g711a(uint8_t *data, int bytes, unsigned short port)
{
	static const char* pattern = "m=audio %hu RTP/AVP 8\n";
	return snprintf((char*)data, bytes, pattern, port);
}
