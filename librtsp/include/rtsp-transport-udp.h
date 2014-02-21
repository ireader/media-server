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
	aio_socket_t m_socket;
};

#endif /* !_rtsp_transport_upd_h_ */
