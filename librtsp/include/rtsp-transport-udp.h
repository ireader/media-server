#ifndef _rtsp_transport_udp_h_
#define _rtsp_transport_udp_h_

#include "rtsp-transport.h"
#include "aio-socket.h"

class RtspTransportUDP : public IRtspTransport
{
public:
	RtspTransportUDP(const char* ip, int port);
	virtual ~RtspTransportUDP();

public:

private:
	void OnRecv(int code, int bytes, const char* ip, int port);
	static void OnRecv(void* param, int code, int bytes, const char* ip, int port);

private:
	char m_buffer[8000];
	aio_socket_t m_socket;
	void *m_parser; // rtsp parser
};

#endif /* !_rtsp_transport_upd_h_ */
