#ifndef _sdp_payload_h_
#define _sdp_payload_h_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int sdp_vp8(uint8_t* data, int bytes, const char* proto, unsigned short port, int payload);
int sdp_vp9(uint8_t* data, int bytes, const char* proto, unsigned short port, int payload);
int sdp_av1(uint8_t* data, int bytes, const char* proto, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
int sdp_h264(uint8_t* data, int bytes, const char* proto, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
int sdp_h265(uint8_t* data, int bytes, const char* proto, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
int sdp_mpeg4_es(uint8_t* data, int bytes, const char* proto, unsigned short port, int payload, int frequence, const void* extra, int extra_size);

int sdp_g711u(uint8_t* data, int bytes, const char* proto, unsigned short port);
int sdp_g711a(uint8_t* data, int bytes, const char* proto, unsigned short port);
int sdp_opus(uint8_t* data, int bytes, const char* proto, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
int sdp_aac_latm(uint8_t* data, int bytes, const char* proto, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
int sdp_aac_generic(uint8_t* data, int bytes, const char* proto, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);

int sdp_mpeg2_ps(uint8_t* data, int bytes, const char* proto, unsigned short port, int payload);
int sdp_mpeg2_ts(uint8_t* data, int bytes, const char* proto, unsigned short port);

/// @param[in] rtp rtp payload, see more @rtp-profile.h and @rtsp-payloads.h
int sdp_payload_video(uint8_t* data, int bytes, int rtp, const char* proto, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
int sdp_payload_audio(uint8_t* data, int bytes, int rtp, const char* proto, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);

#ifdef __cplusplus
}
#endif
#endif /* !_sdp_payload_h_ */
