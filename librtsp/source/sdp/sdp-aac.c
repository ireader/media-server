// RFC6184 RTP Payload Format for H.264 Video

#include "mpeg4-aac.h"
#include "base64.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams
int sdp_aac_latm(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size)
{
	// In the presence of SBR, the sampling rates for the core encoder/
	// decoder and the SBR tool are different in most cases. Therefore,
	// this parameter SHALL NOT be considered as the definitive sampling rate.
	static const char* pattern =
		"m=audio %hu RTP/AVP %d\n"
		"a=rtpmap:%d MP4A-LATM/%d/%d\n"
		"a=fmtp:%d profile-level-id=%d;object=%d;cpresent=0;config=";

	int r, n;
	uint8_t config[6];
	struct mpeg4_aac_t aac;

	//aac.profile = MPEG4_AAC_LC;
	//aac.channel_configuration = (uint8_t)channel_count;
	//aac.sampling_frequency_index = (uint8_t)mpeg4_aac_audio_frequency_from(sample_rate);
	r = mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, extra_size, &aac);
	if (r < 0) return r;
	assert(aac.sampling_frequency_index == (uint8_t)mpeg4_aac_audio_frequency_from(sample_rate));
	assert(aac.channel_configuration == channel_count);

	r = mpeg4_aac_stream_mux_config_save(&aac, config, sizeof(config));
	if (r < 0) return r;

	// the "rate" parameter indicates the RTP timestamp "clock rate". 
	// The default value is 90000. Other rates MAY be indicated
	//	only if they are set to the same value as the audio sampling rate
	sample_rate = 0 == sample_rate ? 90000 : sample_rate;

	n = snprintf((char*)data, bytes, pattern, port,
		payload, payload, sample_rate, channel_count, 
		payload, mpeg4_aac_profile_level(&aac), aac.profile);

	if (n + r * 2 + 1 > bytes)
		return -1; // // don't have enough memory
	n += base16_encode((char*)data + n, config, r);

	if (n < bytes)
		data[n++] = '\n';
	return n;
}

// RFC 3640 3.3.1. General (p21)
int sdp_aac_generic(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size)
{
	// a=rtpmap:<payload type> <encoding name>/<clock rate>[/<encoding parameters > ]
	// For audio streams, <encoding parameters> specifies the number of audio channels
	// streamType: AudioStream
	// When using SDP, the clock rate of the RTP time stamp MUST be expressed using the "rtpmap" attribute. 
	// If an MPEG-4 audio stream is transported, the rate SHOULD be set to the same value as the sampling rate of the audio stream. 
	// If an MPEG-4 video stream transported, it is RECOMMENDED that the rate be set to 90 kHz.
	static const char* pattern =
		"m=audio %hu RTP/AVP %d\n"
		"a=rtpmap:%d MPEG4-GENERIC/%d/%d\n"
		"a=fmtp:%d streamType=5;profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=";

	int n;

	n = snprintf((char*)data, bytes, pattern, port, payload, payload, sample_rate, channel_count, payload);

	if (n + extra_size * 2 + 1 > bytes)
		return -1; // // don't have enough memory

	// For MPEG-4 Audio streams, config is the audio object type specific
	// decoder configuration data AudioSpecificConfig()
	n += base64_encode((char*)data + n, extra, extra_size);

	if (n < bytes)
		data[n++] = '\n';
	return n;
}
