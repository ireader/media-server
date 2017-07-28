#ifndef _rtsp_client_h_
#define _rtsp_client_h_

#include "rtsp-header-transport.h"
#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct rtsp_client_t rtsp_client_t;

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
/// @param[in] usr RTSP auth username(optional)
/// @param[in] pwd RTSP auth password(optional)
rtsp_client_t* rtsp_client_create(const char* usr, const char* pwd, const struct rtsp_client_handler_t *handler, void* param);

void rtsp_client_destroy(rtsp_client_t* rtsp);

/// rtsp describe and setup
/// @param[in] uri media resource uri
/// @param[in] sdp resource info. it can be null, sdp will get by describe command
/// @return 0-ok, -EACCESS-auth required, try again, other-error.
int rtsp_client_open(rtsp_client_t* rtsp, const char* uri, const char* sdp);

/// stop and close session(TearDown)
/// call onclose on done
/// @return 0-ok, other-error.
int rtsp_client_close(rtsp_client_t* rtsp);

/// play session(PLAY)
/// call onplay on done
/// @param[in] npt PLAY range parameter [optional, NULL is acceptable]
/// @param[in] speed PLAY scale+speed parameter [optional, NULL is acceptable]
/// @return 0-ok, other-error.
/// Notice: if npt and speed is null, resume play only
int rtsp_client_play(rtsp_client_t* rtsp, const uint64_t *npt, const float *speed);

/// pause session(PAUSE)
/// call onpause on done
/// @return 0-ok, other-error.
/// use rtsp_client_play(rtsp, NULL, NULL) to resume play
int rtsp_client_pause(rtsp_client_t* rtsp);

int rtsp_client_input(rtsp_client_t* rtsp, void* parser);

int rtsp_client_media_count(rtsp_client_t* rtsp);
const struct rtsp_header_transport_t* rtsp_client_get_media_transport(rtsp_client_t* rtsp, int media);
const char* rtsp_client_get_media_encoding(rtsp_client_t* rtsp, int media);
int rtsp_client_get_media_payload(rtsp_client_t* rtsp, int media);
int rtsp_client_get_media_rate(rtsp_client_t* rtsp, int media); // return 0 if unknown rate

#if defined(__cplusplus)
}
#endif
#endif /* !_rtsp_client_h_ */
