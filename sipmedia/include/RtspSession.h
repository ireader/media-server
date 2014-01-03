#ifndef _RtspSession_h_
#define _RtspSession_h_

#include "libct/auto_obj.h"
#include "libct/auto_ptr.h"
#include "aio-socket.h"
#include <string>

class RtspSession : public libct::object
{
public:
	RtspSession(socket_t sock, const char* ip, int port);
	~RtspSession();

public:
	void Run();

private:
	static void OnRecv(void* param, int code, int bytes);
	static void OnSend(void* param, int code, int bytes);

	void Reply();

private:
	int m_port;
	std::string m_ip;
	aio_socket_t m_socket;

	char m_rxbuffer[5*1024];
	char m_txbuffer[5*1024];
	socket_bufvec_t m_vec[2];
};

#endif /* !_RtspSession_h_ */
