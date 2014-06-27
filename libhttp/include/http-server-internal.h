#ifndef _http_server_internal_h_
#define _http_server_internal_h_

#include "aio-socket.h"
#include "http-server.h"

struct http_session_t
{
	struct http_session_t *prev;
	struct http_session_t *next;

	struct http_server_t *server;

	aio_socket_t socket;
	void* parser;

	char data[2 * 1024];

	// send buffer vector
	int vec_count;
	socket_bufvec_t *vec;
	socket_bufvec_t vec3[3];
};

struct http_server_t
{
	aio_socket_t socket;

//	struct http_session_t head;

	http_server_handler handle;
	void* param;
};

struct http_session_t* http_session_run(struct http_server_t *server, socket_t socket, const char* ip, int port);

#endif /* !_http_server_internal_h_ */
