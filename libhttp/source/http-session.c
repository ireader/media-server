#include "cstringext.h"
#include "sys/sock.h"
#include "http-reason.h"
#include "http-parser.h"
#include "http-bundle.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include "http-server-internal.h"

#if defined(OS_WINDOWS)
#define iov_base buf
#define iov_len	 len
#endif

static int http_session_start(struct http_session_t *session);

// create a new http session
static struct http_session_t* http_session_new()
{
	struct http_session_t* session;

	session = (struct http_session_t*)malloc(sizeof(session[0]));
	if(!session)
		return NULL;

	memset(session, 0, sizeof(session[0]));
	session->parser = http_parser_create(HTTP_PARSER_SERVER);
	return session;
}

static void http_session_drop(struct http_session_t *session)
{
	if(session->socket)
		aio_socket_destroy(session->socket);

	if(session->parser)
		http_parser_destroy(session->parser);

	free(session);
}

// reuse/create a http session
static struct http_session_t* http_session_alloc()
{
	struct http_session_t* session;

	// TODO: reuse ? 
	session = http_session_new();
	if(!session)
		return NULL;

	return session;
}

static void http_session_handle(struct http_session_t *session)
{
	const char* uri = http_get_request_uri(session->parser);
	const char* method = http_get_request_method(session->parser);

	if(session->server->handle)
	{
		session->server->handle(session->server->param, session, method, uri);
	}
}

static void http_session_onrecv(void* param, int code, size_t bytes)
{
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	// peer close socket, don't receive all data
	//if(0 == code && 0 == bytes) 

	if(0 == code && bytes > 0)
	{
		size_t remain = bytes;
		code = http_parser_input(session->parser, session->data, &remain);
		if(0 == code)
		{
			session->data[0] = '\0'; // clear for save user-defined header

			// call
			// user must reply(send/send_vec/send_file) in handle
			http_session_handle(session);

			// do restart in send done
			// http_session_onsend
		}
		else if(1 == code)
		{
			code = aio_socket_recv(session->socket, session->data, sizeof(session->data), http_session_onrecv, session);
		}
	}

	if(code < 0 || 0 == bytes)
	{
		http_session_drop(session);
		printf("http_session_onrecv => %d\n", 0==bytes ? 0 : code);
	}
}

static int http_session_send(struct http_session_t *session, int idx);
static void http_session_onsend(void* param, int code, size_t bytes)
{
	int i;
	char* ptr;
	struct http_session_t *session;
	session = (struct http_session_t*)param;

    printf("http_session_onsend code: %d, bytes: %u\n", code, (unsigned int)bytes);
	if(code < 0 || 0 == bytes)
	{
		http_session_drop(session);
	}
	else
	{
		for(i = 0; i < session->vec_count && bytes > 0; i++)
		{
			if(bytes >= session->vec[i].iov_len)
			{
				bytes -= session->vec[i].iov_len;
				session->vec[i].iov_len = 0;
			}
			else
			{
				ptr = session->vec[i].iov_base;
				session->vec[i].iov_len -= bytes;
				memmove(ptr, ptr + bytes, session->vec[i].iov_len);
				bytes = 0;
				break;
			}
		}

		if(i < session->vec_count)
		{
			http_session_send(session, i);
		}
		else
		{
			// release bundle data
			for(i = 2; i < session->vec_count; i++)
			{
				//http_bundle_free(session->vec[i].buf);
				http_bundle_free((struct http_bundle_t *)session->vec[i].iov_base - 1);
			}

			if(session->vec3 != session->vec)
				free(session->vec);
			session->vec = NULL;
			session->vec_count = 0;

			// restart
			http_session_start(session);
		}
	}
}

static int http_session_send(struct http_session_t *session, int idx)
{
	int r;
	size_t i;
	r = aio_socket_send_v(session->socket, session->vec + idx, session->vec_count-idx, http_session_onsend, session);
	if(0 != r)
	{
		// release bundle data
		for(i = 2; i < session->vec_count; i++)
		{
			http_bundle_free((struct http_bundle_t *)session->vec[i].iov_base - 1);
		}

		// free socket vector
		if(session->vec3 != session->vec)
			free(session->vec);
		session->vec = NULL;
		session->vec_count = 0;

		// drop session
		http_session_drop(session);
	}

	return r;
}

static int http_session_start(struct http_session_t *session)
{
	int r;

	// clear parser status
	http_parser_clear(session->parser);

	// receive client request
	r = aio_socket_recv(session->socket, session->data, sizeof(session->data), http_session_onrecv, session);
	if(0 != r)
	{
		printf("http_session_run recv => %d\n", r);
		http_session_drop(session);
		return -1;
	}

	return 0;
}

struct http_session_t* http_session_run(struct http_server_t *server, socket_t socket, const char* ip, int port)
{
	int r;
	struct http_session_t *session;

	session = http_session_alloc();
	if(!session) return NULL;

	session->server = server;
	session->socket = aio_socket_create(socket, 1);

	r = http_session_start(session);

	return 0==r ? session : NULL;
}

// Request
int http_server_get_host(void* param, void** ip, int *port)
{
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	return 0;
}

const char* http_server_get_header(void* param, const char *name)
{
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	return http_get_header_by_name(session->parser, name);
}

int http_server_get_content(void* param, void **content, size_t *length)
{
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	*content = (void*)http_get_content(session->parser);
	*length = http_get_content_length(session->parser);
	return 0;
}

int http_server_send(void* param, int code, void* bundle)
{
	return http_server_send_vec(param, code, &bundle, 1);
}

int http_server_send_vec(void* param, int code, void** bundles, int num)
{
	int i;
    size_t len;
	char msg[128];
	struct http_bundle_t *bundle;
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	assert(0 == session->vec_count);
	session->vec = (1 == num) ? session->vec3 : malloc((sizeof(session->vec[0]) + sizeof(void*)) * (num+2));
	if(!session->vec)
		return -1;

	session->vec_count = num + 2;

	// HTTP Response Data
	len = 0;
	for(i = 0; i < num; i++)
	{
		bundle = bundles[i];
		assert(bundle->len > 0);
		http_bundle_addref(bundle); // addref
		socket_setbufvec(session->vec, i+2, bundle->ptr, bundle->len);
		len += bundle->len;
	}

	// HTTP Response Header
	sprintf(msg, "Server: WebServer 0.2\r\n"
		"Connection: keep-alive\r\n"
		"Keep-Alive: timeout=5,max=100\r\n"
		"Content-Length: %u\r\n\r\n", (unsigned int)len);
	strcat(session->data, msg);
	sprintf(msg, "HTTP/1.1 %d %s\r\n", code, http_reason_phrase(code));

	socket_setbufvec(session->vec, 0, msg, strlen(msg));
	socket_setbufvec(session->vec, 1, session->data, strlen(session->data));

	return http_session_send(session, 0);
}

int http_server_set_header(void* param, const char* name, const char* value)
{
	char msg[512];
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	snprintf(msg, sizeof(msg), "%s: %s\r\n", name, value);
	strcat(session->data, msg);
	return 0;
}

int http_server_set_header_int(void* param, const char* name, int value)
{
	char msg[512];
	struct http_session_t *session;
	session = (struct http_session_t*)param;

	snprintf(msg, sizeof(msg), "%s: %d\r\n", name, value);
	strcat(session->data, msg);
	return 0;
}

int http_server_set_content_type(void* session, const char* value)
{
	//Content-Type: application/json
	//Content-Type: text/html; charset=utf-8
	return http_server_set_header(session, "Content-Type", value);
}
