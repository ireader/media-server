#ifndef _rtsp_server_internal_h_
#define _rtsp_server_internal_h_

#include "sys/sock.h"
#include "sys/locker.h"
#include "sys/atomic.h"
#include "rtsp-server.h"
#include "rtsp-parser.h"

#define MAX_UDP_PACKAGE 1024

struct rtsp_session_t;

struct rtsp_server_t
{
	struct rtsp_handler_t handler;
	void* param;

	void* udp; // udp transport
	void* tcp; // tcp listen

	void (*onrecv)(struct rtsp_session_t* session);
	void (*onsend)(struct rtsp_session_t* session, int code, size_t bytes);
};

struct rtsp_session_t
{
	int32_t ref;
	locker_t locker;

	rtsp_parser_t* parser; // rtsp parser
	struct rtsp_server_t *server;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	
	unsigned int cseq;
	char reply[MAX_UDP_PACKAGE];

	void* transport;
	int (*send)(void *session, const void* data, size_t bytes);
	int (*sendv)(void *session, socket_bufvec_t *vec, int n);
};

void rtsp_server_handle(struct rtsp_session_t *session);
void rtsp_server_options(struct rtsp_session_t* session, const char* uri);
void rtsp_server_describe(struct rtsp_session_t *session, const char* uri);
void rtsp_server_setup(struct rtsp_session_t* session, const char* uri);
void rtsp_server_play(struct rtsp_session_t *session, const char* uri);
void rtsp_server_pause(struct rtsp_session_t* session, const char* uri);
void rtsp_server_teardown(struct rtsp_session_t *session, const char* uri);
int rtsp_server_reply(struct rtsp_session_t *session, int code);

void* rtsp_server_listen(const char* ip, int port, void* param);
int rtsp_server_unlisten(void* aio);

int rtsp_transport_tcp_create(socket_t socket, const struct sockaddr* addr, socklen_t addrlen, void* param);

void* rtsp_transport_udp_create(const char* ip, int port, void* param);
void rtsp_transport_udp_destroy(void* transport);

#endif /* !_rtsp_server_internal_h_ */
