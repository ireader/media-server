#ifndef _rtsp_header_transport_h_
#define _rtsp_header_transport_h_

#ifdef __cplusplus
extern "C" {
#endif

enum { 
	RTSP_TRANSPORT_UNKNOWN = 0, 
	RTSP_TRANSPORT_UNICAST = 1, 
	RTSP_TRANSPORT_MULTICAST,
};

// transport
enum {
	RTSP_TRANSPORT_RTP = 1,
	RTSP_TRANSPORT_RAW,
};

// transport lower transport
enum {
	RTSP_TRANSPORT_UDP = 1, 
	RTSP_TRANSPORT_TCP
};

// transport mode
enum {
	RTSP_TRANSPORT_PLAY = 1, 
	RTSP_TRANSPORT_RECORD
};

struct rtsp_header_transport_t
{
	int transport; // RTP/RAW
	int lower_transport; // TCP/UDP, RTP/AVP default UDP
	int multicast; // unicast/multicast, default multicast
	char* destination;
	char* source;
	int layer; // rtsp setup response only
	int mode; // PLAY/RECORD, default PLAY, rtsp setup response only
	int append; // use with RECORD mode only, rtsp setup response only
	int interleaved1, interleaved2; // rtsp setup response only
	int ttl; // multicast only
	unsigned short port1, port2; // RTP only
	unsigned short client_port1, client_port2; // unicast RTP/RTCP port pair, RTP only
	unsigned short server_port1, server_port2; // unicast RTP/RTCP port pair, RTP only
	char* ssrc; // RTP only
};

int rtsp_header_transport(const char* fields, struct rtsp_header_transport_t* transport);

#ifdef __cplusplus
}
#endif
#endif /* !_rtsp_header_transport_h_ */
