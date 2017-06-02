#include "rtmp-client-transport.h"
#include "rtmp-client.h"
#include "aio-socket.h"
#include "cstringext.h"
#include "sockutil.h"
#include "app-log.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "sys/atomic.h"
#include "sys/sync.hpp"

enum 
{
	AIO_SEND_IDLE = 0,
	AIO_SEND_PROTOCOL,
	AIO_SEND_AVMEDIA,
};

struct rtmp_client_transport_t
{
	int32_t ref;

	int port;
	char host[128];
	char app[128];
	char stream[128];
	char tcurl[256];

	int publish;

	void* rtmp;
	aio_socket_t socket;
	int aio_send_status; // AIO_SEND_XXX
	bool avbuffer_write;
	bool running;
	bool onready;
	
	// recv buffer
	uint8_t rbuffer[8 * 1024];

	// RTMP Protocol send buffer
	uint8_t sbuffer[2 * 1024];
	size_t sbytes;

	// A/V packet send buffer
	uint8_t* avbuffer;
	size_t avcapacity;
	size_t avbytes;
	
	ThreadLocker locker;

	void* param;
	struct rtmp_client_transport_handler_t handler;
};

class rtmp_client_transport_release
{
	struct rtmp_client_transport_t* t;

public:
	rtmp_client_transport_release(struct rtmp_client_transport_t* t) 
	{
		this->t = t;
	}

	~rtmp_client_transport_release() 
	{
		if (0 == atomic_decrement32(&t->ref))
		{
			delete t;
		}
	}
};

static int rtmp_client_transport_send(struct rtmp_client_transport_t* t);
static int rtmp_client_transport_recv(struct rtmp_client_transport_t* t);
static void rtmp_client_transport_onsend(void* transport, int code, size_t bytes);
static void rtmp_client_transport_onrecv(void* transport, int code, size_t bytes);
static void rtmp_client_transport_onconnect(void* transport, int code);

static void* rtmp_client_transport_alloc(void* transport, int avtype, size_t bytes)
{
	struct rtmp_client_transport_t* t;
	t = (struct rtmp_client_transport_t*)transport;
	return t->handler.alloc(t->param, avtype, bytes);
}

static int rtmp_client_transport_onaudio(void* transport, const void* audio, size_t bytes, uint32_t timestamp)
{
	struct rtmp_client_transport_t* t;
	t = (struct rtmp_client_transport_t*)transport;
	t->handler.onaudio(t->param, audio, bytes, timestamp);
	return 0;
}

static int rtmp_client_transport_onvideo(void* transport, const void* video, size_t bytes, uint32_t timestamp)
{
	struct rtmp_client_transport_t* t;
	t = (struct rtmp_client_transport_t*)transport;
	t->handler.onvideo(t->param, video, bytes, timestamp);
	return 0;
}

static int rtmp_client_transport_onmeta(void* /*transport*/, const void* /*meta*/, size_t /*bytes*/)
{
	return 0; // ignore
}

static int rtmp_client_transport_write(void* transport, const void* header, size_t len, const void* data, size_t bytes)
{
	struct rtmp_client_transport_t* t;
	t = (struct rtmp_client_transport_t*)transport;

	//assert(t->locker.IsLocked());
	//AutoThreadLocker locker(t->locker);
	uint8_t* buffer = t->avbuffer_write ? t->avbuffer : t->sbuffer;
	size_t capacity = t->avbuffer_write ? t->avcapacity : sizeof(t->sbuffer);
	size_t* size = t->avbuffer_write ? &t->avbytes : &t->sbytes;

	if (*size + len + bytes > capacity)
		return -1;

	if (len > 0)
	{
		memcpy(buffer + *size, header, len);
		*size += len;
	}

	if (bytes > 0)
	{
		memcpy(buffer + *size, data, bytes);
		*size += bytes;
	}

	return len + bytes;
}

void* rtmp_client_transport_create(const char* host, int port, const char* app, const char* stream, const char* tcurl, struct rtmp_client_transport_handler_t* handler, void* param)
{
	rtmp_client_transport_t* t = new rtmp_client_transport_t();
	strlcpy(t->host, host, sizeof(t->host));
	strlcpy(t->app, app, sizeof(t->app));
	strlcpy(t->stream, stream, sizeof(t->stream));
	strlcpy(t->tcurl, tcurl, sizeof(t->tcurl));
	t->port = port;

	t->rtmp = NULL;
	t->socket = invalid_aio_socket;
	t->avbuffer_write = false;
	t->aio_send_status = AIO_SEND_IDLE;
	t->avcapacity = t->avbytes = t->sbytes = 0;
	t->avbuffer = NULL;

	memcpy(&t->handler, handler, sizeof(t->handler));
	t->param = param;
	t->ref = 1;
	return t;
}

void rtmp_client_transport_destroy(void* transport)
{
	struct rtmp_client_transport_t* t;
	t = (struct rtmp_client_transport_t*)transport;
	rtmp_client_transport_stop(t);
	rtmp_client_transport_release release(t);
}

static int rtmp_client_transport_send(struct rtmp_client_transport_t* t)
{
	//assert(t->locker.IsLocked());
	//AutoThreadLocker locker(t->locker);
	if (AIO_SEND_IDLE != t->aio_send_status || t->sbytes + t->avbytes < 1)
		return 0; // busy or not data to send

	assert(t->running);
	void* buffer = (void*)(t->sbytes > 0 ? t->sbuffer : t->avbuffer);
	size_t bytes = t->sbytes > 0 ? t->sbytes : t->avbytes;
	t->aio_send_status = t->sbytes > 0 ? AIO_SEND_PROTOCOL : AIO_SEND_AVMEDIA;

	atomic_increment32(&t->ref);
	int r = aio_socket_send(t->socket, buffer, bytes, rtmp_client_transport_onsend, t);
	if (0 != r) atomic_decrement32(&t->ref);
	assert(t->ref > 0);
	return r;
}

static int rtmp_client_transport_recv(struct rtmp_client_transport_t* t)
{
	assert(t->running);
	atomic_increment32(&t->ref);
	int r = aio_socket_recv(t->socket, t->rbuffer, sizeof(t->rbuffer), rtmp_client_transport_onrecv, t);
	if (0 != r) atomic_decrement32(&t->ref);
	assert(t->ref > 0);
	return r;
}

static void rtmp_client_transport_onerror(struct rtmp_client_transport_t* t)
{
	rtmp_client_transport_stop(t);

	// reconnect
	rtmp_client_transport_start(t, t->publish);
}

static void rtmp_client_transport_onsend(void* transport, int code, size_t bytes)
{
	struct rtmp_client_transport_t* t;
	t = (struct rtmp_client_transport_t*)transport;
	rtmp_client_transport_release release(t);

	{
		AutoThreadLocker locker(t->locker);
		assert(AIO_SEND_IDLE != t->aio_send_status);
		int status = t->aio_send_status;
		t->aio_send_status = AIO_SEND_IDLE;
		if (!t->running) return;

		if (0 == code)
		{
			if (AIO_SEND_PROTOCOL == status)
			{
				// some body write more data
				if (t->sbytes > bytes)
					memmove(t->sbuffer, t->sbuffer + bytes, t->sbytes - bytes);
				t->sbytes -= bytes;
			}
			else if (AIO_SEND_AVMEDIA == status)
			{
				t->avbytes -= bytes;
				
				// notify send done
				int r = t->avbytes;
				t->locker.Unlock();
				if (0 == r && t->handler.onsend)
					t->handler.onsend(t->param);
				t->locker.Lock();
			}
			else
			{
				assert(0);
			}

			code = rtmp_client_transport_send(t);
		}
	}

	if (0 != code)
	{
		app_log(LOG_ERROR, "RTMP url(%s/%s) send failed: %d/%d\n", t->tcurl, t->stream, code, socket_geterror());
		rtmp_client_transport_onerror(t);
	}
}

static void rtmp_client_transport_onrecv(void* transport, int code, size_t bytes)
{
	struct rtmp_client_transport_t* t;
	t = (struct rtmp_client_transport_t*)transport;
	rtmp_client_transport_release release(t);

	if (0 == bytes && 0 == code)
	{
		// socket close by peer (maybe no data)
		app_log(LOG_ERROR, "RTMP url(%s/%s) connection close by peer.\n", t->tcurl, t->stream);
		rtmp_client_transport_onerror(t);
		return;
	}

	{
		AutoThreadLocker locker(t->locker);
		if (!t->running) return;

		if (0 == code)
		{
			if (0 == rtmp_client_input(t->rtmp, t->rbuffer, bytes))
				code = rtmp_client_transport_send(t);
		}

		if (0 == t->publish && t->onready && 4/*RTMP_STATE_START*/ == rtmp_client_getstate(t->rtmp))
		{
			t->onready = false;
			if (t->handler.onready && t->running)
				t->handler.onready(t->param);
		}

		// recv more data
		if (0 == code)
			code = rtmp_client_transport_recv(t);
	}

	if (0 != code)
	{
		app_log(LOG_ERROR, "RTMP url(%s/%s) recv error: %d/%d\n", t->tcurl, t->stream, code, socket_geterror());
		rtmp_client_transport_onerror(t);
	}
}

static void rtmp_client_transport_onconnect(void* transport, int code)
{
	struct rtmp_client_transport_t* t;
	t = (struct rtmp_client_transport_t*)transport;
	rtmp_client_transport_release release(t);

	{
		AutoThreadLocker locke(t->locker);
		if (!t->running) return;

		if (0 == code)
		{
			if (0 == rtmp_client_start(t->rtmp, t->publish))
				code = rtmp_client_transport_send(t);
		}

		// start recv
		if (0 == code)
			code = rtmp_client_transport_recv(t);
	}

	if (0 != code)
	{
		app_log(LOG_ERROR, "RTMP url(%s/%s) connect error: %d/%d\n", t->tcurl, t->stream, code, socket_geterror());
		rtmp_client_transport_onerror(t);
	}
}

static int rtmp_client_transport_connect(struct rtmp_client_transport_t* t)
{
	socket_t tcp = socket_tcp();
#if defined(OS_WINDOWS)
	socket_bind_any(tcp, 0);
#endif
	socket_setnonblock(tcp, 1);
	socket_setnondelay(tcp, 1);

	assert(invalid_aio_socket == t->socket);
	t->socket = aio_socket_create(tcp, 1);
	if (invalid_aio_socket == t->socket)
	{
		app_log(LOG_ERROR, "RTMP url(%s/%s) create socket error: %d\n", t->tcurl, t->stream, socket_geterror());
		socket_close(tcp);
		return -1;
	}

	struct sockaddr_storage ss;
	socklen_t len = sizeof(ss);
	if (0 != socket_addr_from(&ss, &len, t->host, (u_short)t->port))
	{
		app_log(LOG_ERROR, "RTMP url(%s/%s) get address error: %d\n", t->tcurl, t->stream, socket_geterror());
		return -1;
	}

	atomic_increment32(&t->ref);
	int r = aio_socket_connect(t->socket, (sockaddr*)&ss, len, rtmp_client_transport_onconnect, t);
	if (0 != r)
	{
		app_log(LOG_ERROR, "RTMP url(%s/%s) connect error: %d/%d\n", t->tcurl, t->stream, r, socket_geterror());
		atomic_decrement32(&t->ref);
		return -1;
	}

	return 0;
}

int rtmp_client_transport_start(void* transport, int publish)
{
	struct rtmp_client_transport_t* t;
	t = (struct rtmp_client_transport_t*)transport;
	t->publish = publish;
	t->onready = true;
	t->running = true;

	assert(NULL == t->rtmp);
	struct rtmp_client_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = rtmp_client_transport_write;
	handler.alloc = rtmp_client_transport_alloc;
	handler.onmeta = rtmp_client_transport_onmeta;
	handler.onaudio = rtmp_client_transport_onaudio;
	handler.onvideo = rtmp_client_transport_onvideo;
	t->rtmp = rtmp_client_create(t->app, t->stream, t->tcurl, t, &handler);
	if (NULL == t->rtmp)
		return -1;

	return rtmp_client_transport_connect(t);
}

int rtmp_client_transport_stop(void* transport)
{
	struct rtmp_client_transport_t* t;
	t = (struct rtmp_client_transport_t*)transport;

	AutoThreadLocker locker(t->locker);
	t->onready = false;
	t->running = false;

	if (invalid_aio_socket != t->socket)
	{
		aio_socket_destroy(t->socket);
		t->socket = invalid_aio_socket;
		t->aio_send_status = AIO_SEND_IDLE;
	}

	if (NULL != t->rtmp)
	{
		rtmp_client_destroy(t->rtmp);
		t->rtmp = NULL;
	}
	return 0;
}

static int rtmp_client_transport_avalloc(rtmp_client_transport_t* t, size_t bytes)
{
	assert(0 == t->avbytes);
	if (t->avcapacity < bytes + 2 * 1024)
	{
		void* p = realloc(t->avbuffer, bytes + 2 * 1024);
		if (NULL == p)
			return ENOMEM;

		t->avbuffer = (uint8_t*)p;
		t->avcapacity = bytes + 2 * 1024;
	}
	return 0;
}

int rtmp_client_transport_sendpacket(void* transport, int avtype, const void* flv, size_t bytes, uint32_t timestamp)
{
	struct rtmp_client_transport_t* t;
	t = (struct rtmp_client_transport_t*)transport;

	int r = rtmp_client_transport_avalloc(t, bytes);
	if (0 != r)
	{
		app_log(LOG_ERROR, "RTMP url(%s/%s) alloc(%d) error\n", t->tcurl, t->stream, (int)bytes);
		return r;
	}
	t->avbytes = 0;

	AutoThreadLocker locke(t->locker);
	t->avbuffer_write = true;
	if (0 == avtype)
	{
		r = rtmp_client_push_audio(t->rtmp, flv, bytes, timestamp);
	}
	else
	{
		r = rtmp_client_push_video(t->rtmp, flv, bytes, timestamp);
	}
	t->avbuffer_write = false;

	if (0 != r)
		return r;
	
	return rtmp_client_transport_send(t);
}
