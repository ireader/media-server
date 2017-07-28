#include "rtsp-server-internal.h"
#include "rfc822-datetime.h"

void rtsp_server_describe(struct rtsp_session_t *session, const char* uri)
{
	struct rtsp_server_t* ctx = session->server;
	ctx->handler.describe(ctx->param, session, uri);
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
void rtsp_server_reply_describe(struct rtsp_session_t *session, int code, const char* sdp)
{
	int len;
	rfc822_datetime_t datetime;

	if (200 != code)
	{
		rtsp_server_reply(session, code);
		return;
	}

	len = snprintf(session->reply, sizeof(session->reply),
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Date: %s\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: %u\r\n"
		"\r\n"
		"%s",
		session->cseq,
		rfc822_datetime_format(time(NULL), datetime),
		(unsigned int)strlen(sdp), sdp);

	session->send(session, session->reply, len);
}
