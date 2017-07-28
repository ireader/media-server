#include "rtsp-server-internal.h"
#include "rtsp-header-session.h"
#include "rtsp-header-transport.h"
#include "rfc822-datetime.h"

static int rtsp_header_transport_ex(const char* value, struct rtsp_header_transport_t *transport, size_t *num)
{
	size_t i;
	const char* p = value;

	for (i = 0; i < *num && p; i++)
	{
		if (0 != rtsp_header_transport(p, &transport[i]))
			return -1;

		p = strchr(p + 1, ',');
	}

	*num = i;
	return 0;
}

void rtsp_server_setup(struct rtsp_session_t* session, const char* uri)
{
	size_t n;
	const char *psession, *ptransport;
	struct rtsp_header_session_t xsession;
	struct rtsp_header_transport_t transport[16];
	struct rtsp_server_t* ctx = session->server;

	psession = rtsp_get_header_by_name(session->parser, "Session");
	ptransport = rtsp_get_header_by_name(session->parser, "Transport");

	memset(transport, 0, sizeof(transport));
	n = sizeof(transport) / sizeof(transport[0]);
	if (!ptransport || 0 != rtsp_header_transport_ex(ptransport, transport, &n) || 0 == n)
	{
		// 461 Unsupported Transport
		rtsp_server_reply(session, 461);
		return;
	}

	assert(n > 0);
	if (psession && 0 == rtsp_header_session(psession, &xsession))
	{
		ctx->handler.setup(ctx->param, session, uri, xsession.session, transport, n);
	}
	else
	{
		ctx->handler.setup(ctx->param, session, uri, NULL, transport, n);
	}
}

void rtsp_server_reply_setup(struct rtsp_session_t *session, int code, const char* sessionid, const char* transport)
{
	int len;
	rfc822_datetime_t datetime;

	if (200 != code)
	{
		rtsp_server_reply(session, code);
		return;
	}

	rfc822_datetime_format(time(NULL), datetime);

	// RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257
	len = snprintf(session->reply, sizeof(session->reply),
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Date: %s\r\n"
		"Session: %s\r\n"
		"Transport: %s\r\n"
		"\r\n",
		session->cseq, datetime, sessionid, transport);

	session->send(session, session->reply, len);
}
