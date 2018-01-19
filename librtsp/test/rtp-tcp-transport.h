#ifndef _rtp_tcp_transport_h_
#define _rtp_tcp_transport_h_

#include "media/media-source.h"

class RTPTcpTransport : public IRTPTransport
{
public:
	RTPTcpTransport() {}
	virtual ~RTPTcpTransport() {}

public:
	virtual int Send(bool rtcp, const void* data, size_t bytes) { return -1; }

private:
};

#endif /* !_rtp_tcp_transport_h_ */
