#include <stdlib.h>
#include <memory.h>
#include "ctypedef.h"
#include "rtsp-server.h"
#include "cstringext.h"
#include "tcpserver.h"
#include "udpsocket.h"
#include "error.h"
#include "rfc822-datetime.h"
#include "rtsp-server-internal.h"
#include "rtsp-transport.h"
#include "rtsp-header-range.h"
#include "rtsp-header-session.h"
#include "rtsp-parser.h"
#include "rtsp-reason.h"

#define MAX_UDP_PACKAGE 1024

struct rtsp_server_context_t
{
	struct rtsp_handler_t handler;
	void* ptr;

	struct rtsp_transport_t *tcptransport;
	struct rtsp_transport_t *udptransport;
	void* udp;
	void* tcp;
};

struct rtsp_server_request_t
{
	struct rtsp_server_context_t *server;
	struct rtsp_transport_t *transport;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	void* session;
	void* parser;
	unsigned int cseq;
	char reply[MAX_UDP_PACKAGE];
};

static int rtsp_header_transport_ex(const char* value, struct rtsp_header_transport_t *transport, size_t *num)
{
	size_t i;
	const char* p = value;

	for(i = 0; i < *num && p; i++)
	{
		if(0 != rtsp_header_transport(p, &transport[i]))
			return -1;

		p = strchr(p+1, ',');
	}

	*num = i;
	return 0;
}

static int rtsp_server_reply(struct rtsp_server_request_t *req, int code)
{
	int len;
	rfc822_datetime_t datetime;
	datetime_format(time(NULL), datetime);

	len = snprintf(req->reply, sizeof(req->reply),
		"RTSP/1.0 %d %s\r\n"
		"CSeq: %u\r\n"
		"Date: %s\r\n"
		"\r\n",
		code, rtsp_reason_phrase(code), req->cseq, datetime);

	return req->transport->send(req->session, req->reply, len);
}

// RFC 2326 10.1 OPTIONS (p30)
static void rtsp_server_options(struct rtsp_server_request_t* req, void* parser, const char* uri)
{
	static const char* methods = "DESCRIBE,SETUP,TEARDOWN,PLAY,PAUSE";
	int len;

//	assert(0 == strcmp("*", uri));
	len = snprintf(req->reply, sizeof(req->reply), 
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Public: %s\r\n"
		"\r\n", 
		req->cseq, methods);

	req->transport->send(req->session, req->reply, len);
}

static void rtsp_server_describe(struct rtsp_server_request_t *req, void* parser, const char* uri)
{
	struct rtsp_server_context_t* ctx = req->server;
	ctx->handler.describe(ctx->ptr, req, uri);
	//rfc822_datetime_t date = {0};
	//srand(time(NULL));
	//unsigned int sid = (unsigned int)rand();
	//uri = rtsp_get_request_uri(parser);
	//snprintf(sdps, sizeof(sdps), 
	//	"v=0\r\n"
	//	"o=- %u %u IN IP4 %s\r\n"
	//	"s=%s\r\n"
	//	"c=IN IP4 %s\r\n"
	//	"t=0 0\r\n", sid, time(NULL), "127.0.0.1");

	//snprintf(sdpmv, sizeof(sdpmv), 
	//	"m=video\r\n"
	//	"a=0 0\r\n");

	//snprintf(sdpma, sizeof(sdpma), 
	//	"m=\r\n"
	//	"a=0 0\r\n");
	//datetime_format(time(NULL), date);

	//snprintf(ctx->req, sizeof(ctx->req), 
	//	"RTSP/1.0 200 OK\r\n"
	//	"CSeq: %u\r\n"
	//	"Date: %s\r\n"
	//	"Content-Type: application/sdp\r\n"
	//	"Content-Length: %d\r\n"
	//	"\r\n", 
	//	seq, date, sdplen);
}

static void rtsp_server_setup(struct rtsp_server_request_t *req, void *parser, const char* uri)
{
	size_t n;
	const char *psession, *ptransport;
	struct rtsp_header_session_t session;
	struct rtsp_header_transport_t transport[16];
	struct rtsp_server_context_t* ctx = req->server;

	psession = rtsp_get_header_by_name(parser, "Session");
	ptransport = rtsp_get_header_by_name(parser, "Transport");

	memset(transport, 0, sizeof(transport));
	n = sizeof(transport)/sizeof(transport[0]);
	if(!ptransport || 0 != rtsp_header_transport_ex(ptransport, transport, &n) || 0 == n)
	{
		// 461 Unsupported Transport
		rtsp_server_reply(req, 461);
		return;
	}

	assert(n > 0);
	if(psession && 0 == rtsp_header_session(psession, &session))
	{
		ctx->handler.setup(ctx->ptr, req, uri, session.session, transport, n);
	}
	else
	{
		ctx->handler.setup(ctx->ptr, req, uri, NULL, transport, n);
	}
}

static void rtsp_server_teardown(struct rtsp_server_request_t *req, void *parser, const char* uri)
{
	const char *psession;
	struct rtsp_header_session_t session;
	struct rtsp_server_context_t* ctx = req->server;

	psession = rtsp_get_header_by_name(parser, "Session");
	if(!psession || 0 != rtsp_header_session(psession, &session))
	{
		// 454 (Session Not Found)
		rtsp_server_reply(req, 454);
		return;
	}

	ctx->handler.teardown(ctx->ptr, req, uri, session.session);
}

static void rtsp_server_play(struct rtsp_server_request_t *req, void* parser, const char* uri)
{
	int64_t npt = -1L;
	double scale = 0.0f;
	const char *pscale, *prange, *psession;
	struct rtsp_header_range_t range;
	struct rtsp_header_session_t session;
	struct rtsp_server_context_t* ctx = req->server;

	pscale = rtsp_get_header_by_name(parser, "scale");
	prange = rtsp_get_header_by_name(parser, "range");
	psession = rtsp_get_header_by_name(parser, "Session");

	if(!psession || 0 != rtsp_header_session(psession, &session))
	{
		// 454 (Session Not Found)
		rtsp_server_reply(req, 454);
		return;
	}

	if(pscale)
	{
		scale = atof(pscale);
	}

	if(prange && 0 == rtsp_header_range(prange, &range))
	{
		npt = range.from;
	}

	ctx->handler.play(ctx->ptr, req, uri, session.session, -1L==npt?NULL:&npt, pscale?&scale:NULL);
}

// RFC2326 10.6 PAUSE (p36)
// 1. A PAUSE request discards all queued PLAY requests. However, the pause
//	  point in the media stream MUST be maintained. A subsequent PLAY
//	  request without Range header resumes from the pause point.
static void rtsp_server_pause(struct rtsp_server_request_t *req, void* parser, const char* uri)
{
	int64_t npt = -1L;
	const char *prange, *psession;
	struct rtsp_header_range_t range;
	struct rtsp_header_session_t session;
	struct rtsp_server_context_t* ctx = req->server;

	prange = rtsp_get_header_by_name(parser, "range");
	psession = rtsp_get_header_by_name(parser, "Session");

	if(!psession || 0 != rtsp_header_session(psession, &session))
	{
		// 454 Session Not Found
		rtsp_server_reply(req, 454);
		return;
	}

	if(prange && 0 == rtsp_header_range(prange, &range))
	{
		npt = range.from;
		// 10.6 The normal play time for the stream is set to the pause point. (p36)
		assert(range.type == RTSP_RANGE_NPT); // 3.6 Normal Play Time (p17)
		assert(range.to_value == RTSP_RANGE_TIME_NOVALUE);

		// 457 Invalid Range
		//rtsp_server_reply(req, 457);
		//return;
	}

	ctx->handler.pause(ctx->ptr, req, uri, session.session, -1L==npt?NULL:&npt);
}

static void rtsp_server_handle(struct rtsp_server_request_t *req, void* parser)
{
	int major, minor;
	const char* uri;
	const char* method;

	req->parser = parser;
	rtsp_get_version(parser, &major, &minor);
	if(1 != major && 0 != minor)
	{
		//505 RTSP Version Not Supported
		rtsp_server_reply(req, 505);
		return;
	}

	if(0 != rtsp_get_header_by_name2(parser, "CSeq", (int*)&req->cseq))
	{
		// 400 Bad Request
		rtsp_server_reply(req, 400);
		return;
	}

	uri = rtsp_get_request_uri(parser);
	method = rtsp_get_request_method(parser);

	switch(*method)
	{
	case 'o':
	case 'O':
		if(0 == strcasecmp("OPTIONS", method))
		{
			rtsp_server_options(req, parser, uri);
			return;
		}
		break;

	case 'd':
	case 'D':
		if(0 == strcasecmp("DESCRIBE", method))
		{
			rtsp_server_describe(req, parser, uri);
			return;
		}
		break;

	case 's':
	case 'S':
		if(0 == strcasecmp("SETUP", method))
		{
			rtsp_server_setup(req, parser, uri);
			return;
		}
		break;

	case 'p':
	case 'P':
		if(0 == strcasecmp("PLAY", method))
		{
			rtsp_server_play(req, parser, uri);
			return;
		}
		else if(0 == strcasecmp("PAUSE", method))
		{
			rtsp_server_pause(req, parser, uri);
			return;
		}
		break;

	case 't':
	case 'T':
		if(0 == strcasecmp("TEARDOWN", method))
		{
			rtsp_server_teardown(req, parser, uri);
			return;
		}
		break;
	}

	// 501 Not implemented
	rtsp_server_reply(req, 501);
}

static struct rtsp_server_request_t* rtsp_server_session_create(struct rtsp_server_context_t *ctx)
{
	struct rtsp_server_request_t* req;

	req = (struct rtsp_server_request_t*)malloc(sizeof(*req));
	if(!req) return NULL;
	memset(req, 0, sizeof(*req));
	req->server = ctx;
	return req;
}

static void rtsp_server_onrecv(void* ptr, void* session, const struct sockaddr* addr, socklen_t addrlen, void* parser, void** user)
{
	struct rtsp_server_context_t *ctx;
	struct rtsp_server_request_t *req;
	ctx = (struct rtsp_server_context_t*)ptr;

	req = rtsp_server_session_create(ctx);
	req->session = session;
	req->transport = ctx->tcptransport;
	req->cseq = (unsigned int)-1;
	assert(addrlen <= sizeof(req->addr));
	memcpy(&req->addr, addr, addrlen);
	req->addrlen = addrlen;

	*user = req;
	rtsp_server_handle(req, parser);
}

static void rtsp_server_onsend(void *ptr, void* user, int code, size_t bytes)
{
	struct rtsp_server_request_t *req;
	req = (struct rtsp_server_request_t *)user;
}

void rtsp_server_reply_describe(void* rtsp, int code, const char* sdp)
{
	int len;
	rfc822_datetime_t datetime;
	struct rtsp_server_request_t *req;
	req = (struct rtsp_server_request_t *)rtsp;

	if(200 != code)
	{
		rtsp_server_reply(req, code);
		return;
	}

	datetime_format(time(NULL), datetime);
	len = snprintf(req->reply, sizeof(req->reply),
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %u\r\n"
			"Date: %s\r\n"
			"Content-Type: application/sdp\r\n"
			"Content-Length: %u\r\n"
			"\r\n"
			"%s", 
			req->cseq, datetime, (unsigned int)strlen(sdp), sdp);

	req->transport->send(req->session, req->reply, len);
}

void rtsp_server_reply_setup(void* rtsp, int code, const char* session, const char* transport)
{
	int len;
	rfc822_datetime_t datetime;
	struct rtsp_server_request_t *req;
	req = (struct rtsp_server_request_t *)rtsp;

	if(200 != code)
	{
		rtsp_server_reply(req, code);
		return;
	}

	datetime_format(time(NULL), datetime);

	// RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257
	len = snprintf(req->reply, sizeof(req->reply),
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %u\r\n"
			"Date: %s\r\n"
			"Session: %s\r\n"
			"Transport: %s\r\n"
			"\r\n",
			req->cseq, datetime, session, transport);

	req->transport->send(req->session, req->reply, len);
}

void rtsp_server_reply_play(void* rtsp, int code, const int64_t *nptstart, const int64_t *nptend, const char* rtp)
{
	int len;
	char range[64] = {0};
	char rtpinfo[256] = {0};
	rfc822_datetime_t datetime;
	struct rtsp_server_request_t *req;
	req = (struct rtsp_server_request_t *)rtsp;

	if(200 != code)
	{
		rtsp_server_reply(req, code);
		return;
	}

	if(rtp)
	{
		len = snprintf(rtpinfo, sizeof(rtpinfo), "RTP-Info: %s\r\n", rtp);
	}

	if(nptstart)
	{
		if(nptend)
			len = snprintf(range, sizeof(range), "Range: %.3f-%.3f\r\n", (float)(*nptstart/1000.0f), (float)(*nptend/1000.0f));
		else
			len = snprintf(range, sizeof(range), "Range: %.3f-\r\n", (float)(*nptstart/1000.0f));
	}

	datetime_format(time(NULL), datetime);
	// smpte=0:10:22-;time=19970123T153600Z
	len = snprintf(req->reply, sizeof(req->reply),
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Date: %s\r\n"
		"%s" // Range
		"%s" // RTP-Info
		"\r\n",
		req->cseq, datetime, range, rtpinfo);

	req->transport->send(req->session, req->reply, len);
}

void rtsp_server_reply_pause(void* rtsp, int code)
{
	struct rtsp_server_request_t *req;
	req = (struct rtsp_server_request_t *)rtsp;
	rtsp_server_reply(req, code);
}

void rtsp_server_reply_teardown(void* rtsp, int code)
{
	struct rtsp_server_request_t *req;
	req = (struct rtsp_server_request_t *)rtsp;
	rtsp_server_reply(req, code);
}

const char* rtsp_server_get_header(void* rtsp, const char* name)
{
	struct rtsp_server_request_t *req;
	req = (struct rtsp_server_request_t *)rtsp;
	return rtsp_get_header_by_name(req->parser, name);
}

int rtsp_server_get_client(void* rtsp, char ip[65], unsigned short *port)
{
	struct rtsp_server_request_t *req;
	req = (struct rtsp_server_request_t *)rtsp;
	if (NULL == ip || NULL == port)
		return -1;
	return socket_addr_to((struct sockaddr*)&req->addr, req->addrlen, ip, port);
}

int rtsp_server_init(void)
{
    return 0;
}

int rtsp_server_cleanup(void)
{
    return 0;
}

void* rtsp_server_create(const char* ip, int port, struct rtsp_handler_t* handler, void* ptr)
{
	socket_t socket;
	struct rtsp_server_context_t* ctx;
	struct rtsp_transport_handler_t tcphandler;
	struct rtsp_transport_handler_t udphandler;

	tcphandler.onrecv = rtsp_server_onrecv;
	tcphandler.onsend = rtsp_server_onsend;
	udphandler.onrecv = rtsp_server_onrecv;
	udphandler.onsend = rtsp_server_onsend;

	ctx = (struct rtsp_server_context_t *)malloc(sizeof(struct rtsp_server_context_t));
	if(!ctx) return NULL;

	memset(ctx, 0, sizeof(struct rtsp_server_context_t));
	memcpy(&ctx->handler, handler, sizeof(ctx->handler));
	ctx->tcptransport = rtsp_transport_tcp();
	ctx->udptransport = rtsp_transport_udp();
	ctx->ptr = ptr;

	// tcp
	socket = tcpserver_create(ip, port, 128);
	if(socket_invalid == socket)
	{
		rtsp_server_destroy(ctx);
		return NULL;
	}
	ctx->tcp = ctx->tcptransport->create(socket, &tcphandler, ctx);
	if(!ctx->tcp)
	{
		socket_close(socket);
		rtsp_server_destroy(ctx);
		return NULL;
	}

	// udp
	socket = udpsocket_create(ip, port);
	if(socket_invalid == socket)
	{
		rtsp_server_destroy(ctx);
		return NULL;
	}
	ctx->udp = ctx->udptransport->create(socket, &udphandler, ctx);
	if(!ctx->udp)
	{
		socket_close(socket);
		rtsp_server_destroy(ctx);
		return NULL;
	}

	return ctx;
}

int rtsp_server_destroy(void* server)
{
	struct rtsp_server_context_t* ctx;
	ctx = (struct rtsp_server_context_t*)server;

	if(ctx->tcp)
		ctx->tcptransport->destroy(ctx->tcp);
	if(ctx->udp)
		ctx->udptransport->destroy(ctx->udp);

	free(ctx);
	return 0;
}

int rtsp_server_report(void* server)
{
	return 0;
}
