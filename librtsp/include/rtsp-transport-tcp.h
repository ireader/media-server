#ifndef _rtsp_transport_tcp_h_
#define _rtsp_transport_tcp_h_

#include "rtsp-transport.h"
#include "aio-socket.h"
#include <map>
#include <string>

class TransportSession;
class RtspTransportTCP : public IRtspTransport
{
public:
	RtspTransportTCP(const char* ip, int port);
	virtual ~RtspTransportTCP();

public:

private:
	static void OnAccept(void* param, int code, socket_t socket, const char* ip, int port);

private:
	typedef std::map<std::string, TransportSession*> TSessions;
	TSessions m_sessions;

	aio_socket_t m_socket;
};

#endif /* !_rtsp_transport_tcp_h_ */
