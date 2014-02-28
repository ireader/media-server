#include "rtsp-server.h"
#include "cstringext.h"
#include "sys/sock.h"
#include "sys/sync.h"
#include "sys/system.h"
#include "aio-socket.h"
#include "time64.h"
#include "tcpserver.h"
#include "error.h"
#include "rfc822-datetime.h"

#define N_TRANSPORT 2

struct rtsp_context
{
	void* transports[2];
	locker_t locker;
};

void* rtsp_server_create()
{
	struct rtsp_context* ctx;
	ctx = (struct rtsp_context *)malloc(sizeof(struct rtsp_context));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(struct rtsp_context));
	locker_create(&ctx->locker);
	return ctx;
}

int rtsp_server_destroy(void* server)
{
	struct rtsp_context* ctx;
	ctx = (struct rtsp_context*)server;
	locker_destroy(&ctx->locker);
	return 0;
}

int rtsp_server_add_transport(void* server, IRtspTransport* transport)
{
	struct rtsp_context* ctx;
	ctx = (struct rtsp_context*)server;
	ctx->transports.push_back(transport);
	return 0;
}

int rtsp_server_delete_transport(void* server, IRtspTransport* transport)
{
	struct rtsp_context* ctx;
	ctx = (struct rtsp_context*)server;
	std::list<IRtspTransport*>::iterator it;
	it = std::find(ctx->transports.begin(), ctx->transports.end(), transport);
	if(it == ctx->transports.end())
		return ERROR_NOTFOUND;
	ctx->transports.erase(it);
	return 0;
}

int rtsp_server_report(void* server)
{
}

static int rtsp_server_setup(struct rtsp_context* ctx, void* parser)
{
}

static int rtsp_server_teardown(struct rtsp_context* ctx, void* parser)
{
}

static int rtsp_server_play(struct rtsp_context* ctx, void* parser)
{
}

static int rtsp_server_pause(struct rtsp_context* ctx, void* parser)
{
}

static int rtsp_server_option(struct rtsp_context* ctx, void* parser)
{
	static const char* methods = "DESCRIBE SETUP TEARDOWN PLAY PAUSE";
	int seq = 0;
	rtsp_get_header_by_name2(parser, "CSeq", &seq);

	snprintf(ctx->req, sizeof(ctx->req), 
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Public: %s\r\n"
		"\r\n", 
		seq, methods);

	reply(ctx->transports, "");
}

static int rtsp_server_describe(struct rtsp_context* ctx, void* parser)
{
	const char* uri;
	char date[27] = {0};

	srand(time(NULL));
	unsigned int sid = (unsigned int)rand();
	uri = rtsp_get_request_uri(parser);
	snprintf(sdps, sizeof(sdps), 
		"v=0\r\n"
		"o=- %u %u IN IP4 %s\r\n"
		"s=%s\r\n"
		"c=IN IP4 %s\r\n"
		"t=0 0\r\n", sid, time(NULL), "127.0.0.1");

	snprintf(sdpmv, sizeof(sdpmv), 
		"m=video\r\n"
		"a=0 0\r\n");

	snprintf(sdpma, sizeof(sdpma), 
		"m=\r\n"
		"a=0 0\r\n");
	datetime_format(time(NULL), date);

	snprintf(ctx->req, sizeof(ctx->req), 
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Date: %s\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: %d\r\n"
		"\r\n", 
		seq, date, sdplen);

	reply(ctx->transports, "");
}

static int rtsp_server_get_parameter(struct rtsp_context* ctx, void* parser)
{
}

static int rtsp_server_set_parameter(struct rtsp_context* ctx, void* parser)
{
}

static int rtsp_server_announce(struct rtsp_context* ctx, void* parser)
{
}

static int rtsp_server_record(struct rtsp_context* ctx, void* parser)
{
}

static int rtsp_server_redirect(struct rtsp_context* ctx, void* parser)
{
}

static void rtsp_server_handle(void* server, void* transport, void* parser)
{
	const char* uri;
	const char* method;
	struct rtsp_context* ctx;
	ctx = (struct rtsp_context*)server;

	uri = rtsp_get_request_uri(parser);
	method = rtsp_get_request_method(parser);
	// call method
}
