#include "rtsp-transport.h"
#include "rtsp-transport-tcp.h"
#include "rtsp-transport-udp.h"

void* rtsp_transport_create(int transport, const char* ip, int port)
{
	IRtspTransport* t = NULL;

	switch(transport)
	{
	case RTSP_TRANSPORT_TCP:
		t = new RtspTransportTCP(ip, port);
		break;

	case RTSP_TRANSPORT_UDP:
		t = new RtspTransportUDP(ip, port);
		break;
	}

	return t;
}

int rtsp_transport_destroy(void* transport)
{
	IRtspTransport* t = (IRtspTransport*)transport;
	delete t;
	return 0;
}
