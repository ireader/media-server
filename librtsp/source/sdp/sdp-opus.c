// RFC7587 RTP Payload Format for the Opus Speech and Audio Codec

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "sdp-payload.h"

int sdp_opus(uint8_t *data, int bytes, const char* proto, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size)
{
	/* The opus RTP draft says that all opus streams MUST be declared
	as stereo, to avoid negotiation failures. The actual number of
	channels can change on a packet-by-packet basis. The number of
	channels a receiver prefers to receive or a sender plans to send
	can be declared via fmtp parameters (both default to mono), but
	receivers MUST be able to receive and process stereo packets. */
	static const char* pattern =
		"m=audio %hu %s %d\n"
		"a=rtpmap:%d opus/%d/2\n";

	int n;

	sample_rate = sample_rate ? sample_rate : 48000;
	n = snprintf((char*)data, bytes, pattern, port, proto && *proto ? proto : "RTP/AVP", payload, payload, sample_rate);
	if (2 == channel_count)
		n += snprintf((char*)data + n, bytes - n, "a=fmtp:%d sprop-stereo=1\n", payload);
	return n;
}
