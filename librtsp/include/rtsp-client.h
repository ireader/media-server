#ifndef _rtsp_client_h_
#define _rtsp_client_h_

#include <stdint.h>
#include <stddef.h>
#include "rtsp-header-transport.h"

#if defined(__cplusplus)
extern "C" {
#endif

// seq=232433;rtptime=972948234
struct rtsp_rtp_info_t
{
	const char* uri;
	uint32_t seq;	// uint16_t
	uint32_t time;	// uint32_t
};

struct rtsp_client_handler_t
{
	///network implementation
	///@return >0-sent bytes, <0-error
	int (*send)(void* param, const char* uri, const void* req, size_t bytes);
	///create rtp/rtcp port 
	int (*rtpport)(void* param, unsigned short *rtp); // udp only(rtp%2=0 and rtcp=rtp+1), rtp=0 if you want to use RTP over RTSP(tcp mode)

	void (*onopen)(void* param);
	void (*onclose)(void* param);
	void (*onplay)(void* param, int media, const uint64_t *nptbegin, const uint64_t *nptend, const double *scale, const struct rtsp_rtp_info_t* rtpinfo, int count); // play
	void (*onpause)(void* param);
};

/// @param[in] param user-defined parameter
void* rtsp_client_create(const struct rtsp_client_handler_t *handler, void* param);

void rtsp_client_destroy(void* rtsp);

/// rtsp describe and setup
/// @param[in] uri media resource uri
/// @param[in] sdp resource info. it can be null, sdp will get by describe command
/// @return 0-ok, other-error.
int rtsp_client_open(void* rtsp, const char* uri, const char* sdp);

/// stop and close session(TearDown)
/// call onclose on done
/// @return 0-ok, other-error.
int rtsp_client_close(void* rtsp);

/// play session(PLAY)
/// call onplay on done
/// @param[in] npt PLAY range parameter [optional, NULL is acceptable]
/// @param[in] speed PLAY scale+speed parameter [optional, NULL is acceptable]
/// @return 0-ok, other-error.
/// Notice: if npt and speed is null, resume play only
int rtsp_client_play(void* rtsp, const uint64_t *npt, const float *speed);

/// pause session(PAUSE)
/// call onpause on done
/// @return 0-ok, other-error.
/// use rtsp_client_play(rtsp, NULL, NULL) to resume play
int rtsp_client_pause(void* rtsp);

int rtsp_client_input(void* rtsp, void* parser);

int rtsp_client_media_count(void* rtsp);
const struct rtsp_header_transport_t* rtsp_client_get_media_transport(void* rtsp, int media);
const char* rtsp_client_get_media_encoding(void* rtsp, int media);
int rtsp_client_get_media_payload(void* rtsp, int media);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtsp_client_h_ */
