#ifndef _rtsp_client_h_
#define _rtsp_client_h_

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(OS_WINDOWS)
	typedef __int64				int64_t;
#else
	typedef long long			int64_t;
#endif

struct rtsp_transport_t
{
	int transport; // 1-AVP/RTP/UDP, 2-AVP/RTP/TCP, 3-RAW-TCP, 4-MP2T/RTP/UDP
	int multicast; // 0-unicast, 1-multicast
	int client_port[2]; // rtp unicast client rtp/rtcp port
	// VLC don't support below parameter
	int server_port[2]; // rtp unicast server rtp/rtcp port or multicast port
	const char *destination; // client address or multicast address
	const char *source; // server address, null if source is rtsp server
	int ttl;			// multicast TTL
	int interleaved[2];	// rtp over rtsp only(TCP mode)
	const char* rtpssrc; // RTP SSRC
};

// seq=232433;rtptime=972948234
struct rtsp_rtp_info_t
{
	const char* uri;
	unsigned int seq;	// uint16_t
	unsigned int time;	// uint32_t
};

typedef void (*rtsp_onreply)(void* rtsp, int code, void* parser);

typedef struct _rtsp_client_t
{
	int (*request)(void* transport, const char* uri, const void* req, int bytes, void* rtsp, rtsp_onreply onreply);
	int (*rtpport)(void* transport, unsigned short *rtp); // udp only(rtp%2=0 and rtcp=rtp+1), rtp=0 if you want to use RTP over RTSP(tcp mode)

	int (*onopen)(void* ptr, int code, const struct rtsp_transport_t* transport, int count);
	int (*onclose)(void* ptr, int code);
	int (*onplay)(void* ptr, int code, const int64_t *nptbegin, const int64_t *nptend, const double *scale, const struct rtsp_rtp_info_t* rtpinfo, int count); // play
	int (*onpause)(void* ptr, int code);
} rtsp_client_t;

void* rtsp_client_create(const rtsp_client_t *client, void* transport, void* ptr);
int rtsp_client_destroy(void* rtsp);

/// rtsp describe and setup
int rtsp_client_open(void* rtsp, const char* uri);

/// rtsp setup only(skip describe)
int rtsp_client_open_with_sdp(void* rtsp, const char* uri, const char* sdp);

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
int rtsp_client_play(void* rtsp, const int64_t *npt, const float *speed);

/// pause session(PAUSE)
/// call onpause on done
/// @return 0-ok, other-error.
/// use rtsp_client_play(rtsp, NULL, NULL) to resume play
int rtsp_client_pause(void* rtsp);

int rtsp_client_media_count(void* rtsp);
const char* rtsp_client_media_get_encoding(void* rtsp, int media);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtsp_client_h_ */
