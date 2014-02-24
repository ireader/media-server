#ifndef _rtsp_transport_h_
#define _rtsp_transport_h_

typedef unsigned int rtspid_t;

struct IRtspTransport
{
	virtual ~IRtspTransport(){}

	virtual int Send() = 0;
	virtual int Recv() = 0;
	virtual int SendV() = 0;
	virtual int RecvV() = 0;
};


enum { RTSP_TRANSPORT_TCP, RTSP_TRANSPORT_UDP };

void* rtsp_transport_create(int type, const char* ip, int port);

int rtsp_transport_destroy(void* transport);

#endif /* !_rtsp_transport_h_ */
