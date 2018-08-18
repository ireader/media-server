// RFC7587 RTP Payload Format for the Opus Speech and Audio Codec

#include "mpeg4-aac.h"
#include "base64.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int sdp_g711u(uint8_t *data, int bytes, int payload, int sample_rate, int channel_count, const void* extra, int extra_size)
{
	static const char* pattern =
		"m=audio 0 RTP/AVP %d\n"
		"a=rtpmap:%d PCMU/%d/%d\n";

	return snprintf((char*)data, bytes, pattern, payload, payload, sample_rate, channel_count);
}
