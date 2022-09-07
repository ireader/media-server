// RFC6184 RTP Payload Format for H.264 Video

#include "mpeg4-aac.h"
#include "sdp-payload.h"
#include "base64.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams
int sdp_aac_latm(uint8_t *data, int bytes, const char* proto, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size)
{
	// In the presence of SBR, the sampling rates for the core encoder/
	// decoder and the SBR tool are different in most cases. Therefore,
	// this parameter SHALL NOT be considered as the definitive sampling rate.
	// profile-level-id --> ISO/IEC 14496-3:2009 audioProfileLevelIndication values
	static const char* pattern =
		"m=audio %hu %s %d\n"
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
	
	sample_rate = 0 == sample_rate ? mpeg4_aac_audio_frequency_to(aac.sampling_frequency_index) : sample_rate;
	channel_count = 0 == channel_count ? aac.channel_configuration : channel_count;
	assert(aac.sampling_frequency_index == (uint8_t)mpeg4_aac_audio_frequency_from(sample_rate));
	assert(aac.channel_configuration == channel_count);

	r = mpeg4_aac_stream_mux_config_save(&aac, config, sizeof(config));
	if (r < 0) return r;

	// the "rate" parameter indicates the RTP timestamp "clock rate". 
	// The default value is 90000. Other rates MAY be indicated
	//	only if they are set to the same value as the audio sampling rate
	sample_rate = 0 == sample_rate ? 90000 : sample_rate;

	n = snprintf((char*)data, bytes, pattern, port, 
		proto && *proto ? proto : "RTP/AVP",
		payload, payload, sample_rate, channel_count, 
		payload, mpeg4_aac_profile_level(&aac), aac.profile);

	if (n + r * 2 + 1 > bytes)
		return -ENOMEM; // don't have enough memory
	n += (int)base16_encode((char*)data + n, config, r);

	if (n < bytes)
		data[n++] = '\n';
	return n;
}

// RFC 3640 3.3.1. General (p21)
int sdp_aac_generic(uint8_t *data, int bytes, const char* proto, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size)
{
	// a=rtpmap:<payload type> <encoding name>/<clock rate>[/<encoding parameters > ]
	// For audio streams, <encoding parameters> specifies the number of audio channels, default value is 1.
	
	// streamType: AudioStream --> ISO/IEC 14496-1:2010 streamType Values
	// profile-level-id --> ISO/IEC 14496-3:2009 audioProfileLevelIndication values
	// When using SDP, the clock rate of the RTP time stamp MUST be expressed using the "rtpmap" attribute. 
	// If an MPEG-4 audio stream is transported, the rate SHOULD be set to the same value as the sampling rate of the audio stream. 
	// If an MPEG-4 video stream transported, it is RECOMMENDED that the rate be set to 90 kHz.
	static const char* pattern =
		"m=audio %hu %s %d\n"
		"a=rtpmap:%d mpeg4-generic/%d/%d\n"
		"a=fmtp:%d streamtype=5;profile-level-id=%d;mode=AAC-hbr;sizeLength=13;indexLength=3;indexDeltaLength=3;config=";

	int r, n;
	struct mpeg4_aac_t aac;

	r = mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, extra_size, &aac);
	if (r < 0) return r;

	sample_rate = 0 == sample_rate ? mpeg4_aac_audio_frequency_to(aac.sampling_frequency_index) : sample_rate;
	channel_count = 0 == channel_count ? aac.channel_configuration : channel_count;
	assert(aac.sampling_frequency_index == (uint8_t)mpeg4_aac_audio_frequency_from(sample_rate));
	assert(aac.channel_configuration == channel_count);

	n = snprintf((char*)data, bytes, pattern, port, proto && *proto ? proto : "RTP/AVP", payload, payload, sample_rate, channel_count, payload, mpeg4_aac_profile_level(&aac));

	if (n + extra_size * 2 + 1 > bytes)
		return -ENOMEM; // don't have enough memory

	// For MPEG-4 Audio streams, config is the audio object type specific
	// decoder configuration data AudioSpecificConfig()
	n += (int)base16_encode((char*)data + n, extra, extra_size);

	if (n < bytes)
		data[n++] = '\n';
	return n;
}

/// @return >0-ok, <=0-error
int sdp_aac_latm_load(uint8_t* data, int bytes, const char* config)
{
	int n;
	uint8_t buf[128];
	struct mpeg4_aac_t aac;

	n = (int)strlen(config);
	if (n / 2 > sizeof(buf))
		return -E2BIG;

	n = (int)base16_decode(buf, config, n);
	n = mpeg4_aac_stream_mux_config_load(buf, n, &aac);
	return n <= 0 ? n : mpeg4_aac_audio_specific_config_save(&aac, data, bytes);
}

/// @return >0-ok, <=0-error
int sdp_aac_mpeg4_load(uint8_t* data, int bytes, const char* config)
{
	int n;
	n = (int)strlen(config);
	if (n / 2 > bytes)
		return -E2BIG;
	
	return (int)base16_decode(data, config, n);
}

#if defined(_DEBUG) || defined(DEBUG)
void sdp_aac_test(void)
{
	// rfc3640
	//const unsigned char celpcbr[] = { "440E00" }; // 16000/1  streamtype=5; profile-level-id=14
	//const unsigned char celpvbr[] = { "440F20" }; // 16000/1 streamtype=5; profile-level-id=14
	//const unsigned char aaclbr[] = { "1388" }; // 22050/1 streamtype=5; profile-level-id=14
	//const unsigned char aachbr[] = { "11B0" }; // 48000/6 streamtype=5; profile-level-id=16

	const uint8_t config[] = { 0x11, 0x90, };
	//const char* mpeg4_generic_sdp = "m=audio 0 RTP/AVP 96\na=rtpmap:96 mpeg4-generic/48000/2\na=fmtp:96 streamtype=5; profile-level-id=15; mode=AAC-hbr; config=1190; SizeLength=13; IndexLength=3; IndexDeltaLength=3; Profile=1\n";
	const char* mpeg4_generic_sdp = "m=audio 0 RTP/AVP 96\na=rtpmap:96 mpeg4-generic/48000/2\na=fmtp:96 streamtype=5;profile-level-id=41;mode=AAC-hbr;sizeLength=13;indexLength=3;indexDeltaLength=3;config=1190\n";
	//const char* mp4a_latm_sdp = "m=audio 0 RTP/AVP 96\na=rtpmap:96 MP4A-LATM/48000/2\na=fmtp:96 profile-level-id=9;object=8;cpresent=0;config=9128B1071070\n";
	uint8_t buffer[256];
	//struct mpeg4_aac_t aac;
	//int n;

	assert((int)strlen(mpeg4_generic_sdp) == sdp_aac_generic(buffer, sizeof(buffer), "RTP/AVP", 0, 96, 48000, 2, config, sizeof(config)));
	assert(0 == memcmp(mpeg4_generic_sdp, buffer, strlen(mpeg4_generic_sdp)));
	assert(sizeof(config) == sdp_aac_mpeg4_load(buffer, sizeof(buffer), "1190"));
	assert(0 == memcmp(config, buffer, sizeof(config)));
}
#endif
