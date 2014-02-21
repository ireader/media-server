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

#endif /* !_rtsp_transport_h_ */
