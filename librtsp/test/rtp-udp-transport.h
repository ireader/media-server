#ifndef _rtp_udp_transport_h_
#define _rtp_udp_transport_h_

#include "sys/sock.h"
#include "media/media-source.h"

class RTPUdpTransport : public IRTPTransport
{
public:
	RTPUdpTransport();
	virtual ~RTPUdpTransport();

public:
	virtual int Send(bool rtcp, const void* data, size_t bytes);

public:
	int Init(const char* ip, unsigned short port[2]);

private:
	socket_t m_socket[2];
	socklen_t m_addrlen[2];
	struct sockaddr_storage m_addr[2];
};

#endif /* !_rtp_udp_transport_h_ */
