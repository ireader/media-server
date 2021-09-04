#ifndef _rtsp_media_h_
#define _rtsp_media_h_

#include "rtsp-header-range.h"

#if defined(__cplusplus)
extern "C" {
#endif

// RFC4585 Extended RTP Profile for Real-time Transport Control Protocol (RTCP)-Based Feedback (RTP/AVPF)
// RFC5104 Codec Control Messages in the RTP Audio-Visual Profile with Feedback (AVPF)
struct rtcp_fb_t
{
	int fb_id;
	int trr_int;
	char ack[8]; // rpsi,app
	char nack[8]; // pli,sli,rpsi,app
	char ccm[8]; // fir,tmmbr,tstr,vbcm
};

// RFC3611 RTP Control Protocol Extended Reports (RTCP XR)
struct rtcp_xr_t
{
	int loss; // pkt-loss-rle
	int dup; // pkt-dup-rle
	int rcpt_times; // pkt-rcpt-times
	int rcvr_rtt;
	int rcvr_rtt_mode; // 0-sender, 1-all
	int stat_summary; // 0x01-loss, 0x02-dup, 0x04-jitt, 0x08-TTL
	int voip_metrics;
};

// rfc 5576
struct sdp_ssrc_t
{
	uint32_t ssrc;

	// TODO: 
	// ssrc attribute(s)
	// multiple ssrc
	// ssrc-group
};

struct sdp_candidate_t
{
	char foundation[33];
	char transport[8]; // UDP
	char candtype[8];
	uint16_t component; // rtp/rtcp component id, [1, 256]
	uint16_t port;
	uint32_t priority; // [1, 2**31 - 1]
	char address[65];
	char reladdr[65];
	uint16_t relport;
};

// https://tools.ietf.org/html/draft-ietf-mmusic-ice-sip-sdp-39
// https://www.tech-invite.com/fo-abnf/tinv-fo-abnf-sdpatt-rfc5245.html
// a=candidate:3 1 UDP 1862270847 10.95.49.227 63259 typ prflx raddr 192.168.137.93 rport 7078
struct sdp_ice_t
{
	char *pwd; // [22,256]
	char *ufrag; // [4, 256]
	int lite;
	int pacing;
	int mismatch;

	struct sdp_candidate_t *candidates[64];
	struct sdp_candidate_t *remotes[64];
	int candidate_count;
	int remote_count;
};

struct rtsp_media_t
{
	char uri[256]; // rtsp setup url
	char session_uri[256]; // rtsp session url(aggregate control url), empty if non-aggregate control
	
	//unsigned int cseq; // rtsp sequence, unused if aggregate control available
	int64_t start, stop; // sdp t: NTP time since 1900
	char network[16], addrtype[16], address[64], source[64]; // sdp c: connection, address: unicast-source address, mulitcast-multicast address(RFC4566 4.1.  Media and Transport Information)
	struct rtsp_header_range_t range;

	char media[32]; //audio, video, text, application, message
	char proto[64]; // udp, RTP/AVP, RTP/SAVP, RTP/AVPF
	int nport, port[8]; // rtcp-mux: port[0] == port[1]
	int mode; // SDP_A_SENDRECV/SDP_A_SENDONLY/SDP_A_RECVONLY/SDP_A_INACTIVE, @sdp.h
	int setup; // SDP_A_SETUP_ACTPASS/SDP_A_SETUP_ACTIVE/SDP_A_SETUP_PASSIVE, @sdp-utils.h

	int avformat_count;
	struct avformat_t
	{
		int fmt; // RTP payload type
		int rate; // RTP payload frequency
		int channel; // RTP payload channel
		char encoding[64]; // RTP payload encoding
		char *fmtp; // RTP fmtp value
		struct rtcp_fb_t fb;
	} avformats[16];

	struct rtcp_xr_t xr;
	struct sdp_ice_t ice;
	struct sdp_ssrc_t ssrc; // rfc 5576

	int offset;
	char ptr[8 * 1024]; // RTP fmtp value
};

/// @return <0-error, >count-try again, other-ok
int rtsp_media_sdp(const char* sdp, int len, struct rtsp_media_t* medias, int count);

/// Update session and media control url
/// @param[in] base The RTSP Content-Base field
/// @param[in] location The RTSP Content-Location field
/// @param[in] request The RTSP request URL
/// return 0-ok, other-error
int rtsp_media_set_url(struct rtsp_media_t* media, const char* base, const char* location, const char* request);

/// create media sdp
/// @return -0-no media, >0-ok, <0-error
int rtsp_media_to_sdp(const struct rtsp_media_t* m, char* line, int bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtsp_media_h_ */
