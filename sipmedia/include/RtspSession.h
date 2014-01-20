#ifndef _RtspSession_h_
#define _RtspSession_h_

#include "aio-socket.h"
#include "libct/object.h"
#include "libct/auto_obj.h"
#include <string>

class RtspSession : public libct::object
{
public:
	RtspSession(socket_t sock, const char* ip, int port);
	~RtspSession();

public:
	void Run();

private:
	void OnApi();
	void Options();
	void Describe();
	void Announce();
	void Setup();
	void Play();
	void Pause();
	void Teardown();
	void GetParameter();
	void SetParameter();
	void Redirect();
	void Record();
	void Embedded();

private:
	static void OnRecv(void* param, int code, int bytes);
	static void OnSend(void* param, int code, int bytes);

	void Reply(const char* contentType, const void* data, int bytes);
	void Reply(int code, const char* msg);

	std::string GetClient() const;

private:
	int m_port;
	std::string m_ip;
	aio_socket_t m_socket;
	void *m_rtsp;

	const char* m_method;
	const void* m_content;
	int m_contentLength;
	unsigned int m_cseq;

	char m_rxbuffer[5*1024];
	char m_txbuffer[5*1024];
	socket_bufvec_t m_vec[2];
};

#endif /* !_RtspSession_h_ */
