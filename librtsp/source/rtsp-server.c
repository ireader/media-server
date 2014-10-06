#include <stdlib.h>
#include <memory.h>
#include "ctypedef.h"
#include "rtsp-server.h"
#include "cstringext.h"
#include "sys/sock.h"
#include "sys/sync.h"
#include "sys/system.h"
#include "sys/process.h"
#include "thread-pool.h"
#include "aio-socket.h"
#include "tcpserver.h"
#include "time64.h"
#include "error.h"
#include "rfc822-datetime.h"
#include "rtsp-server-internal.h"
#include "rtsp-transport.h"
#include "rtsp-header-range.h"
#include "rtsp-header-session.h"
#include "rtsp-parser.h"
#include "udpsocket.h"
//#include "http-reason.h"

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
	struct rtsp_transport_t *transport;
	struct rtsp_server_context_t *server;
	void* session;
	unsigned int cseq;
	char req[1024];
};

// RFC 2326 10.1 OPTIONS (p30)
static int rtsp_server_options(struct rtsp_server_request_t* rtsp, const char* uri)
{
	static const char* methods = "DESCRIBE,SETUP,TEARDOWN,PLAY,PAUSE";

	assert(0 == strcmp("*", uri));
	snprintf(rtsp->req, sizeof(rtsp->req), 
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Public: %s\r\n"
		"\r\n", 
		rtsp->cseq, methods);

	return rtsp->transport->send(rtsp->session, rtsp->req, strlen(rtsp->req));
}

static int rtsp_server_describe(struct rtsp_server_request_t *rtsp, const char* uri)
{
	struct rtsp_server_context_t* ctx;
	ctx = rtsp->server;
	ctx->handler.describe(ctx->ptr, rtsp, uri);
	//char date[27] = {0};
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

static int rtsp_server_setup(struct rtsp_server_request_t *rtsp, const char* uri)
{
	struct rtsp_server_context_t* ctx = rtsp->server;
	ctx->handler.setup(ctx->ptr, rtsp, uri);
}

static int rtsp_server_teardown(struct rtsp_server_request_t *rtsp, const char* uri)
{
	struct rtsp_server_context_t* ctx = rtsp->server;
	ctx->handler.teardown(ctx->ptr, rtsp, uri);
}

static int rtsp_server_play(struct rtsp_server_request_t *rtsp, void* parser, const char* uri)
{
	int64_t npt = -1L;
	float scale = 0.0f;
	const char *pscale, *prange, *psession;
	struct rtsp_header_range_t range;
	struct rtsp_header_session_t session;
	struct rtsp_server_context_t* ctx = rtsp->server;

	pscale = rtsp_get_header_by_name(parser, "scale");
	prange = rtsp_get_header_by_name(parser, "range");
	psession = rtsp_get_header_by_name(parser, "Session");

	if(!psession || 0 != rtsp_header_session(psession, &session))
	{
		// 454 (Session Not Found)
	}

	if(pscale)
	{
		scale = atof(pscale);
	}

	if(prange && 0 == rtsp_header_range(prange, &range))
	{
		npt = range.from;
	}

	ctx->handler.play(ctx->ptr, rtsp, session, -1L==npt?NULL:&npt, pscale?&scale:NULL);
}

// RFC2326 10.6 PAUSE (p36)
// 1. A PAUSE request discards all queued PLAY requests. However, the pause
//	  point in the media stream MUST be maintained. A subsequent PLAY
//	  request without Range header resumes from the pause point.
static int rtsp_server_pause(struct rtsp_server_request_t *rtsp, void* parser, const char* uri, const char* session)
{
	int64_t npt = -1L;
	float scale = 0.0f;
	const char *pscale, *prange, *psession;
	struct rtsp_header_range_t range;
	struct rtsp_header_session_t session;
	struct rtsp_server_context_t* ctx = rtsp->server;

	prange = rtsp_get_header_by_name(parser, "range");
	psession = rtsp_get_header_by_name(parser, "Session");

	if(!psession || 0 != rtsp_header_session(psession, &session))
	{
		// 454 (Session Not Found)
	}

	if(prange && 0 == rtsp_header_range(prange, &range))
	{
		npt = range.from;
		assert(range.type == RTSP_RANGE_NPT); // 3.6 Normal Play Time (p17)
		assert(range.to_value == RTSP_RANGE_TIME_NOVALUE);

		// "457 Invalid Range"
	}

	ctx->handler.pause(ctx->ptr, rtsp, uri);
}

static void* rtsp_server_onrecv(struct rtsp_server_request_t *session, const char* ip, int port, void* parser)
{
	int major, minor;
	const char* uri;
	const char* method;

	rtsp_get_version(parser, &major, &minor);
	if(1 != major && 0 != minor)
	{
		//505 RTSP Version Not Supported
		return;
	}

	if(0 != rtsp_get_header_by_name2(parser, "CSeq", (int*)&session->cseq))
	{
		// "400" ; Bad Request
	}

	uri = rtsp_get_request_uri(parser);
	method = rtsp_get_request_method(parser);

	switch(*method)
	{
	case 'o':
	case 'O':
		if(0 == stricmp("OPTIONS", method))
		{
			return rtsp_server_options(session, parser, uri);
		}
		break;

	case 'd':
	case 'D':
		if(0 == stricmp("DESCRIBE", method))
		{
			rtsp_server_describe(session, uri);
		}
		break;

	case 's':
	case 'S':
		if(0 == stricmp("SETUP", method))
		{
		}
		break;

	case 'p':
	case 'P':
		if(0 == stricmp("PLAY", method))
		{
		}
		else if(0 == stricmp("PAUSE", method))
		{
		}
		break;

	case 't':
	case 'T':
		if(0 == stricmp("TEARDOWN", method))
		{
		}
		break;
	}

	// 501 Not implemented

	return session;
}

static struct rtsp_server_request_t* rtsp_server_session_create(struct rtsp_server_context_t *ctx)
{
	struct rtsp_server_request_t* rtsp;

	rtsp = (struct rtsp_server_request_t*)malloc(sizeof(*rtsp));
	memset(rtsp, 0, sizeof(rtsp));
	rtsp->server = ctx;
	return rtsp;
}

static void* rtsp_server_ontcprecv(void* ptr, void* session, const char* ip, int port, const void* parser)
{
	struct rtsp_server_context_t *ctx;
	struct rtsp_server_request_t *rtspsession;
	ctx = (struct rtsp_server_context_t*)ptr;

	rtspsession = rtsp_server_session_create(ctx);
	rtspsession->session = session;
	rtspsession->transport = ctx->tcptransport;

	return rtsp_server_onrecv(rtspsession, ip, port, parser);
}

static void* rtsp_server_onudprecv(void* ptr, void* session, const char* ip, int port, const void* parser)
{
	struct rtsp_server_context_t *ctx;
	struct rtsp_server_request_t *rtspsession;
	ctx = (struct rtsp_server_context_t*)ptr;

	rtspsession = rtsp_server_session_create(ctx);
	rtspsession->session = session;
	rtspsession->transport = ctx->udptransport;

	return rtsp_server_onrecv(rtspsession, ip, port, parser);
}

static void rtsp_server_onsend(struct rtsp_server_request_t *rtsp, int code, size_t bytes)
{
	struct rtsp_server_context_t *ctx = rtsp->server;
}

static void rtsp_server_ontcpsend(void *ptr, void* session, int code, size_t bytes)
{
	struct rtsp_server_request_t *rtsp;
	rtsp = (struct rtsp_server_request_t *)session;
	rtsp_server_onsend(rtsp, code, bytes);
}

static void rtsp_server_onudpsend(void *ptr, void* session, int code, size_t bytes)
{
	struct rtsp_server_request_t *rtsp;
	rtsp = (struct rtsp_server_request_t *)session;
	rtsp_server_onsend(rtsp, code, bytes);
}

int rtsp_server_reply(struct rtsp_server_request_t *rtsp, int code)
{
	char datetime[27];
	datetime_format(time(NULL), datetime);

	snprintf(rtsp->req, sizeof(rtsp->req), 
		"RTSP/1.0 %d %s\r\n"
		"CSeq: %u\r\n"
		"Date: %s\r\n"
		"\r\n",
		code, http_reason_phrase(code), rtsp->cseq, datetime);

	return rtsp->transport->send(rtsp->session, rtsp->req, strlen(rtsp->req));
}

int rtsp_server_reply_describe(void* transport, int code, const char* sdp)
{
	char datetime[27];
	struct rtsp_server_request_t *rtsp;
	rtsp = (struct rtsp_server_request_t *)transport;

	if(200 != code)
		return rtsp_server_reply(rtsp, code);

	datetime_format(time(NULL), datetime);
	snprintf(rtsp->req, sizeof(rtsp->req), 
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %u\r\n"
			"Date: %s\r\n"
			"Content-Type: application/sdp\r\n"
			"Content-Length: %u\r\n"
			"\r\n"
			"%s", 
			rtsp->cseq, datetime, strlen(sdp), sdp);	

	return rtsp->transport->send(rtsp->session, rtsp->req, strlen(rtsp->req));
}

int rtsp_server_reply_setup(void* transport, int code, const char* session)
{
	char datetime[27];
	struct rtsp_server_request_t *rtsp;
	rtsp = (struct rtsp_server_request_t *)transport;

	if(200 != code)
		return rtsp_server_reply(rtsp, code);

	datetime_format(time(NULL), datetime);

	// RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257
	snprintf(rtsp->req, sizeof(rtsp->req), 
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %u\r\n"
			"Date: %s\r\n"
			"Session: %s\r\n"
			"Transport: %s\r\n"
			"\r\n",
			rtsp->cseq, datetime, session, transport);

	rtsp->transport->send(rtsp->session, rtsp->req, strlen(rtsp->req));
}

int rtsp_server_reply_play(void* transport, int code, const char* session, const char* rtpinfo)
{
	char datetime[27];
	struct rtsp_server_request_t *rtsp;
	rtsp = (struct rtsp_server_request_t *)transport;

	if(200 != code)
		return rtsp_server_reply(rtsp, code);

	datetime_format(time(NULL), datetime);

	// smpte=0:10:22-;time=19970123T153600Z
	snprintf(rtsp->req, sizeof(rtsp->req), 
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Date: %s\r\n"
		"Range: %s\r\n"
		"RTP-Info: %s\r\n"
		"\r\n",
		rtsp->cseq, datetime, session, transport);

	rtsp->transport->send(rtsp->session, rtsp->req, strlen(rtsp->req));
}

int rtsp_server_reply_pause(void* transport, int code, const char* session)
{
	struct rtsp_server_request_t *rtsp;
	rtsp = (struct rtsp_server_request_t *)transport;
	return rtsp_server_reply(rtsp, code);
}

int rtsp_server_reply_teardown(void* transport, int code, const char* session)
{
	struct rtsp_server_request_t *rtsp;
	rtsp = (struct rtsp_server_request_t *)transport;
	return rtsp_server_reply(rtsp, code);
}

void* rtsp_server_create(const char* ip, int port, struct rtsp_handler_t* handler, void* ptr)
{
	socket_t socket;
	struct rtsp_server_context_t* ctx;
	struct rtsp_transport_handler_t tcphandler;
	struct rtsp_transport_handler_t udphandler;

	tcphandler.onrecv = rtsp_server_ontcprecv;
	tcphandler.onsend = rtsp_server_ontcpsend;
	udphandler.onrecv = rtsp_server_ontcprecv;
	udphandler.onsend = rtsp_server_ontcpsend;

	ctx = (struct rtsp_server_context_t *)malloc(sizeof(struct rtsp_server_context_t));
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
