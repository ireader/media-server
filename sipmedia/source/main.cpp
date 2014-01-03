#include "cstringext.h"
#include "sys/sock.h"
#include "sys/system.h"
#include "RtspSession.h"
#include "aio-socket.h"
#include "thread-pool.h"
#include "tcpserver.h"
#include <stdio.h>

thread_pool_t g_thpool;
static aio_socket_t s_tcp;
static aio_socket_t s_udp;

static void AioWorker(void *param)
{
	while(1)
	{
		aio_socket_process(2*60*1000);
	}
}

void OnAccept(void*, int code, socket_t socket, const char* ip, int port)
{
	if(0 != code)
	{
		printf("aio socket accept error: %d/%d.\n", code, socket_geterror());
		exit(1);
	}

	printf("aio socket accept %s.%d\n", ip, port);

	// listen
	aio_socket_accept(s_tcp, OnAccept, NULL);

	RtspSession* session = new RtspSession(socket, ip, port);
	session->Run();
}

static int udpserver_create(const char* ip, int port)
{
	int ret;
	socket_t socket;
	struct sockaddr_in addr;

	// new a UDP socket
	socket = socket_udp();
	if(socket_error == socket)
		return 0;

	// reuse addr
	socket_setreuseaddr(socket, 1);

	// bind
	if(ip && ip[0])
	{
		ret = socket_addr_ipv4(&addr, ip, (unsigned short)port);
		if(0 == ret)
			ret = socket_bind(socket, (struct sockaddr*)&addr, sizeof(addr));
	}
	else
	{
		ret = socket_bind_any(socket, (unsigned short)port);
	}

	if(0 != ret)
	{
		socket_close(socket);
		return 0;
	}

	return socket;
}

static int StartUdpServer(const char* ip, int port)
{
	socket_t sock = udpserver_create(ip, port);
	if(0 == sock)
	{
		printf("udp-server bind at %s.%d error: %d\n", ip?ip:"127.0.0.1", port, socket_geterror());
		return -1;
	}

	s_udp = aio_socket_create(sock, 1);
	aio_socket_recvfrom(s_udp, s_rxbuffer, sizeof(s_rxbuffer)-1, OnUdpRecv, NULL);
}

static int StartTcpServer(const char* ip, int port)
{
	socket_t sock = tcpserver_create(ip, port, 256);
	if(0 == sock)
	{
		printf("server listen at %s.%d error: %d\n", ip?ip:"127.0.0.1", port, socket_geterror());
		return -1;
	}

	printf("server listen at %s:%d\n", ip?ip:"localhost", port);
	s_tcp = aio_socket_create(sock, 1);
	return aio_socket_accept(s_tcp, OnAccept, NULL); // start server
}

int main(int argc, char* argv[])
{
#if defined(OS_LINUX)
	/* ignore pipe signal */
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, 0);
	sigaction(SIGPIPE, &sa, 0);
#endif

	int port = 554;
	for(int i=1; i<argc; i++)
	{
		if(streq(argv[i], "--port") && i+1<argc)
		{
			port = atoi(argv[++i]);
		}
	}

	size_t cpu = system_getcpucount();
	g_thpool = thread_pool_create(cpu, cpu, cpu*4);

	aio_socket_init(cpu * 2);
	for(int i=0; i<cpu * 2; i++)
		thread_pool_push(g_thpool, AioWorker, NULL); // start worker

	// start server
	StartTcpServer(NULL, port); 
	StartUdpServer(NULL, port);

	for(char c = getchar(); 'q' != c ; c = getchar())
	{
	}

	aio_socket_destroy(s_server);
	aio_socket_clean();
	return 0;
}
