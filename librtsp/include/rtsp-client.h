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
    /// @param[in] source media source address
    /// @param[in,out] port [INPUT] media port, [OUTPUT] udp: bind local port for rtp/rtcp(port[0] % 2 == 0), tcp: channel interleaved id
    /// @param[in,out] ip [INPUT] media address, [OUTPUT] udp bind local ip address, 224~239.x.x.x for multicast udp transport
    /// @param[in] len ip buffer length in byte
    /// @return <0-error, 0-ignore this media track, other-RTSP_TRANSPORT_XXX
	int (*rtpport)(void* param, int media, const char* source, unsigned short port[2], char* ip, int len);

	/// rtsp_client_announce callback only
	int (*onannounce)(void* param);

	/// call rtsp_client_setup
	int (*ondescribe)(void* param, const char* sdp, int len);

	/// @param[in] timeout session timeout in seconds
	/// @param[in] duration -1-unknown or live stream, other-rtsp stream duration in MS
	int (*onsetup)(void* param, int timeout, int64_t duration);
	int (*onplay)(void* param, int media, const uint64_t *nptbegin, const uint64_t *nptend, const double *scale, const struct rtsp_rtp_info_t* rtpinfo, int count); // play
    int (*onrecord)(void* param, int media, const uint64_t *nptbegin, const uint64_t *nptend, const double *scale, const struct rtsp_rtp_info_t* rtpinfo, int count); // record
	int (*onpause)(void* param);
	int (*onteardown)(void* param);

	void (*onrtp)(void* param, uint8_t channel, const void* data, uint16_t bytes);
};

/// @param[in] param user-defined parameter
/// @param[in] usr RTSP auth username(optional)
/// @param[in] pwd RTSP auth password(optional)
rtsp_client_t* rtsp_client_create(const char* uri, const char* usr, const char* pwd, const struct rtsp_client_handler_t *handler, void* param);

void rtsp_client_destroy(rtsp_client_t* rtsp);

/// input server reply
/// @param[in] data server response message
/// @param[in] bytes data length in byte
int rtsp_client_input(rtsp_client_t* rtsp, const void* data, size_t bytes);

/// find RTSP response header
/// @param[in] name header name
/// @return header value, NULL if not found.
/// NOTICE: call in rtsp_client_handler_t callback only
const char* rtsp_client_get_header(rtsp_client_t* rtsp, const char* name);

/// rtsp options (optional)
/// @param[in] commands optional required command, NULL if none
int rtsp_client_options(struct rtsp_client_t* rtsp, const char* commands);

/// rtsp describe (optional)
int rtsp_client_describe(struct rtsp_client_t* rtsp);

/// rtsp setup
/// @param[in] sdp resource info. it can be null, sdp will get by describe command
/// @return 0-ok, -EACCESS-auth required, try again, other-error.
int rtsp_client_setup(rtsp_client_t* rtsp, const char* sdp, int len);

/// stop and close session(TearDown)
/// call onclose on done
/// @return 0-ok, other-error.
int rtsp_client_teardown(rtsp_client_t* rtsp);

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

/// announce server sdp
/// @return 0-ok, other-error.
int rtsp_client_announce(rtsp_client_t* rtsp, const char* sdp);

/// record session(publish)
/// call onrecord on done
/// @param[in] npt RECORD range parameter [optional, NULL is acceptable]
/// @param[in] scale RECORD scale parameter [optional, NULL is acceptable]
/// @return 0-ok, other-error.
/// Notice: if npt and scale is null, resume record only
int rtsp_client_record(struct rtsp_client_t *rtsp, const uint64_t *npt, const float *scale);

/// SDP API
int rtsp_client_media_count(rtsp_client_t* rtsp);
const struct rtsp_header_transport_t* rtsp_client_get_media_transport(rtsp_client_t* rtsp, int media);
const char* rtsp_client_get_media_encoding(rtsp_client_t* rtsp, int media);
const char* rtsp_client_get_media_fmtp(rtsp_client_t* rtsp, int media);
int rtsp_client_get_media_payload(rtsp_client_t* rtsp, int media);
/// @return media sampling rate/frequency, 0 if unknown
int rtsp_client_get_media_rate(rtsp_client_t* rtsp, int media);
/// @return SDP_M_MEDIA_AUDIO/SDP_M_MEDIA_VIDEO/SDP_M_MEDIA_xxx, see more @sdp.h
int rtsp_client_get_media_type(rtsp_client_t* rtsp, int media);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtsp_client_h_ */
