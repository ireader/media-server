#ifndef _sdp_a_webrtc_h_
#define _sdp_a_webrtc_h_

#include <inttypes.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ICE_UFRAG_LEN 255
#define ICE_PASSWORD_LEN 255

#define RTP_PORT_INACTIVE	0 // inactive
#define RTP_PORT_UNDEFINED	9 // port by ice candidate

enum OPTIONAL_BOOL { OPTIONAL_BOOL_UNSET = -1, OPTIONAL_BOOL_FALSE = 0, OPTIONAL_BOOL_TRUE = 1, };

// sdp c: connection, address: unicast-source address, mulitcast-multicast address(RFC4566 4.1.  Media and Transport Information)
struct sdp_address_t
{
	char network[16]; // IN
	char addrtype[16]; // IP4/IP6
	char address[64]; // ip or dns
	char source[64]; // multicast address
	int port[2]; // rtcp-mux: port[0] == port[1]
};

// RFC4585 Extended RTP Profile for Real-time Transport Control Protocol (RTCP)-Based Feedback (RTP/AVPF)
// RFC5104 Codec Control Messages in the RTP Audio-Visual Profile with Feedback (AVPF)
struct sdp_rtcp_fb_t
{
	int fmt; // payload id, -1 for all(-1 ==> * / all)
	char feedback[16]; // ack/nack/ccm

	char param[64]; // ack:rpsi,app; nack: pli,sli,rpsi,app; ccm: fir,tmmbr,tstr,vbcm
	int trr_int;
};

// RFC3611 RTP Control Protocol Extended Reports (RTCP XR)
struct sdp_rtcp_xr_t
{
	int loss; // pkt-loss-rle
	int dup; // pkt-dup-rle
	int rcpt_times; // pkt-rcpt-times
	int rcvr_rtt;
	int rcvr_rtt_mode; // 0-sender, 1-all
	int stat_summary; // 0x01-loss, 0x02-dup, 0x04-jitt, 0x08-TTL
	int voip_metrics;
};

// https://tools.ietf.org/html/draft-ietf-mmusic-ice-sip-sdp-39
// https://www.tech-invite.com/fo-abnf/tinv-fo-abnf-sdpatt-rfc5245.html
// a=candidate:3 1 UDP 1862270847 10.95.49.227 63259 typ prflx raddr 192.168.137.93 rport 7078
struct sdp_ice_candidate_t
{
	char foundation[33];
	char transport[8]; // UDP
	char candtype[8];
	uint16_t component; // rtp/rtcp component id, [1, 256]
	uint16_t port;
	uint32_t priority; // [1, 2**31 - 1]
	char address[65];

	char** extensions; // include raddr/rport/generation
	int nextension;

	// extersions
	char reladdr[65];
	uint16_t relport;
	int generation; // google webrtc externsion
};

struct sdp_simulcast_t
{
	char** sends;
	char** recvs;

	int nsend;
	int nrecv;
};

struct sdp_ssrc_group_t
{
	char key[64];

	char** values;
	int count;
};

struct sdp_rid_t
{
	char rid[65];
	char direction[8];

	// rid-pt-param-list
	char** payloads;
	int npayload;

	int width; // max-width
	int height; // max-height
	int fps; // max-fps
	int fs; // max-fs
	int br; // max-br
	int pps; // max-pps
	double bpp; // max-bpp

	char** depends;
	int ndepend;

	// rid-param-list(include pt/width/height/fps/fs/br/pps/bpp/depend/...)
	char** params;
	int nparam;
};

/// @param[out] direction sendonly/receonly/sendrecv/inactive, default sendrecv
/// @return 0-ok, other-error
int sdp_a_extmap(const char* s, int n, int* ext, int* direction, char url[128]);

/// free(*param)
/// @return 0-ok, other-error
int sdp_a_fmtp(const char* s, int n, int* fmt, char **param);

/// @return 0-ok, other-error
int sdp_a_fingerprint(const char* s, int n, char hash[16], char fingerprint[128]);

/// free(groups)
/// @return 0-ok, other-error
int sdp_a_group(const char* s, int n, char semantics[32], char*** groups, int* count);

/// free(c->extensions)
/// @return 0-ok, other-error
int sdp_a_ice_candidate(const char* s, int n, struct sdp_ice_candidate_t* c);

/// @return 0-ok, other-error
int sdp_a_ice_remote_candidates(const char* s, int n, struct sdp_ice_candidate_t* c);

/// @return 0-ok, other-error
int sdp_a_ice_pacing(const char* s, int n, int* pacing);

/// @param[out] options free(options)
/// @param[out] count options number
/// @return 0-ok, other-error
int sdp_a_ice_options(const char* s, int n, char*** options, int* count);

/// @return 0-ok, other-error
int sdp_a_msid(const char* s, int n, char id[65], char appdata[65]);

/// @return 0-ok, other-error
int sdp_a_mid(const char* s, int n, char tag[256]);

/// @param[out] orient portrait / landscape / seascape
/// @return 0-ok, other-error
int sdp_a_orient(const char* s, int n, char orient[16]);

/// free(rid->params) / free(rid->depends) / free(rid->payloads)
/// @return 0-ok, other-error
int sdp_a_rid(const char* s, int n, struct sdp_rid_t* rid);

/// @return 0-ok, other-error
int sdp_a_rtcp(const char* s, int n, struct sdp_address_t* addr);

/// @return 0-ok, other-error
int sdp_a_rtcp_fb(const char* s, int n, struct sdp_rtcp_fb_t* fb);

/// free(simulcast->sends) / free(simulcast->recvs)
/// @return 0-ok, other-error
int sdp_a_simulcast(const char* s, int n, struct sdp_simulcast_t* simulcast);

/// @return 0-ok, other-error
int sdp_a_ssrc(const char* s, int n, uint32_t *ssrc, char attribute[64], char value[128]);

/// free(group->values)
/// @return 0-ok, other-error
int sdp_a_ssrc_group(const char* s, int n, struct sdp_ssrc_group_t* group);

#ifdef __cplusplus
}
#endif
#endif /* !_sdp_a_webrtc_h_ */
