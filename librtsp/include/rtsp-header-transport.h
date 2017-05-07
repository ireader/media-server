#ifndef _rtsp_header_transport_h_
#define _rtsp_header_transport_h_

#ifdef __cplusplus
extern "C" {
#endif

enum {
	RTSP_TRANSPORT_UNICAST = 1, 
	RTSP_TRANSPORT_MULTICAST,
};

// transport
enum {
	RTSP_TRANSPORT_RTP_UDP = 1,
	RTSP_TRANSPORT_RTP_TCP,
	RTSP_TRANSPORT_RAW,
};

// transport mode
enum {
	RTSP_TRANSPORT_PLAY = 1, 
	RTSP_TRANSPORT_RECORD
};

// Transport: RTP/AVP/TCP;interleaved=0-1
// Transport: RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257
// Transport: RTP/AVP;multicast;destination=224.2.0.1;port=3456-3457;ttl=16
struct rtsp_header_transport_t
{
	int transport; // RTSP_TRANSPORT_xxx
	int multicast; // 0-unicast/1-multicast, default multicast
	char destination[65]; // IPv4/IPv6
	char source[65]; // IPv4/IPv6
	int layer; // rtsp setup response only
	int mode; // PLAY/RECORD, default PLAY, rtsp setup response only
	int append; // use with RECORD mode only, rtsp setup response only
	int interleaved1, interleaved2; // rtsp setup response only
	union rtsp_header_transport_rtp_u
	{
		struct rtsp_header_transport_multicast_t
		{
			int ttl; // multicast only
			unsigned short port1, port2; // multicast only
		} m;

		struct rtsp_header_transport_unicast_t
		{
			unsigned short client_port1, client_port2; // unicast RTP/RTCP port pair, RTP only
			unsigned short server_port1, server_port2; // unicast RTP/RTCP port pair, RTP only
			int ssrc; // RTP only(synchronization source (SSRC) identifier) 4-bytes
		} u;
	} rtp;
};

/// parse RTSP Transport header
/// @return 0-ok, other-error
/// usage 1:
/// struct rtsp_header_transport_t transport;
/// const char* header = "Transport: RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257";
/// r = rtsp_header_transport("RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257", &transport);
/// check(r)
/// 
/// usage 2:
/// const char* header = "Transport: RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257,RTP/AVP;unicast;client_port=5000-5001;server_port=6000-6001";
/// split(header, ',');
/// r1 = rtsp_header_transport("RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257", &transport);
/// r2 = rtsp_header_transport("RTP/AVP;unicast;client_port=5000-5001;server_port=6000-6001", &transport);
/// check(r1, r2)
int rtsp_header_transport(const char* field, struct rtsp_header_transport_t* transport);

#ifdef __cplusplus
}
#endif
#endif /* !_rtsp_header_transport_h_ */
