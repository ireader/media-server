#ifndef _sdp_options_h_
#define _sdp_options_h_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sdp_options_t
{
	int bundle;
	int rtcp_mux; // rtp/rtcp port muxer
	
	struct
	{
		int media; // SDP_M_MEDIA_AUDIO
		int setup; // SDP_A_SETUP_ACTPASS
		int proto; // SDP_M_PROTO_RTP_AVP
		int port[2];
		uint8_t payloads[16]; // payload list
	} m[8];
	int count; // media count
};

enum 
{
	SDP_A_SETUP_NONE,
	SDP_A_SETUP_ACTPASS,	// client/server
	SDP_A_SETUP_PASSIVE,	// server
	SDP_A_SETUP_ACTIVE,		// client
	SDP_A_SETUP_HOLDCONN,	// holdconn
};

/// @return SDP_A_SETUP_NONE/SDP_A_SETUP_PASSIVE/SDP_A_SETUP_ACTIVE/SDP_A_SETUP_xxx
int sdp_option_setup_from(const char* setup);
const char* sdp_option_setup_to(int setup);

// RTP      = xxxxxxx1 xxxxxxxx
// UDP      = xxxxxxxx xxxxx000 
// TCP		= xxxxxxxx xxxxx001 
// DCCP		= xxxxxxxx xxxxx010
// SCTP		= xxxxxxxx xxxxx011
// Feedback = xxxxxxxx xxx1xxxx
// SRTP		= xxxxxxxx xx1xxxxx
// TLS		= xxxxxxxx x1xxxxxx
// DTLS		= xxxxxxxx 1xxxxxxx
#define SDP_M_PROTO_TEST_RTP(proto)		( (proto) & 0x0100 )
#define SDP_M_PROTO_TEST_UDP(proto)		( 0x000 == ((proto) & 0x07) ? 1 : 0 )
#define SDP_M_PROTO_TEST_TCP(proto)		( 0x001 == ((proto) & 0x07) ? 1 : 0 )
#define SDP_M_PROTO_TEST_FEEDBACK(proto)( (proto) & 0x10 )
#define SDP_M_PROTO_TEST_SRTP(proto)	( (proto) & 0x20 )
#define SDP_M_PROTO_TEST_TLS(proto)		( (proto) & 0x40 )
#define SDP_M_PROTO_TEST_DTLS(proto)	( (proto) & 0x80 )

enum 
{
	SDP_M_PROTO_UKNOWN				= 0,

	SDP_M_PROTO_RTP_AVP				= 0x0100, // rfc4566: RTP/AVP or RTP/AVP/UDP
	SDP_M_PROTO_RTP_AVPF			= 0x0110, // rfc4585: RTP/AVPF
	SDP_M_PROTO_RTP_SAVP			= 0x0120, // rfc3711: RTP/SAVP
	SDP_M_PROTO_RTP_SAVPF			= 0x0130, // rfc5124: RTP/SAVPF
	SDP_M_PROTO_RTP_AVP_TCP			= 0x0101, // rfc4571: TCP/RTP/AVP or RTP/AVP/TCP or RTP/TCP/AVP
	SDP_M_PROTO_RTP_AVPF_TCP		= 0x0111, // rfc7850: TCP/RTP/AVPF
	SDP_M_PROTO_RTP_SAVP_TCP		= 0x0121, // rfc7850: TCP/RTP/SAVP
	SDP_M_PROTO_RTP_SAVPF_TCP		= 0x0131, // rfc7850: TCP/RTP/SAVPF
	SDP_M_PROTO_RTP_SAVP_DTLS_TCP	= 0x01A1, // rfc7850: TCP/DTLS/RTP/SAVP
	SDP_M_PROTO_RTP_SAVPF_DTLS_TCP	= 0x01B1, // rfc7850: TCP/DTLS/RTP/SAVPF
	SDP_M_PROTO_RTP_AVP_TCP_TLS		= 0x0141, // rfc7850: TCP/TLS/RTP/AVP
	SDP_M_PROTO_RTP_AVPF_TCP_TLS	= 0x0151, // rfc7850: TCP/TLS/RTP/AVPF
	SDP_M_PROTO_RTP_SAVP_TLS		= 0x0160, // rfc5764: UDP/TLS/RTP/SAVP
	SDP_M_PROTO_RTP_SAVPF_TLS		= 0x0170, // rfc5764: UDP/TLS/RTP/SAVPF
	SDP_M_PROTO_RTP_SAVP_TLS_DCCP	= 0x0162, // rfc5764: DCCP/TLS/RTP/SAVP
	SDP_M_PROTO_RTP_SAVPF_TLS_DCCP	= 0x0172, // rfc5764: DCCP/TLS/RTP/SAVPF
	
	SDP_M_PROTO_RAW					= 0x0200, // raw 
	SDP_M_PROTO_UDP					= 0x0300, // rfc4566/rfc8866: udp
	SDP_M_PROTO_SCTP_DTLS			= 0x0480, // rfc8841: UDP/DTLS/SCTP
	SDP_M_PROTO_SCTP_DTLS_TCP		= 0x0481, // rfc8841: TCP/DTLS/SCTP
	SDP_M_PROTO_TCP					= 0x0001, // rfc4145: TCP
	SDP_M_PROTO_TLS_TCP				= 0x0041, // rfc8122: TCP/TLS
};

/// @return SDP_M_PROTO_RAW/SDP_M_PROTO_UDP/SDP_M_PROTO_RTP_AVP/SDP_M_PROTO_XXX
int sdp_option_proto_from(const char* proto);
const char* sdp_option_proto_to(int proto);

int sdp_option_mode_from(const char* mode);
const char* sdp_option_mode_to(int mode);

int sdp_option_media_from(const char* media);
const char* sdp_option_media_to(int media);

#ifdef __cplusplus
}
#endif
#endif /* !_sdp_options_h_ */
