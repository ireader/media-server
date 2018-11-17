/*
C->S: 
TEARDOWN rtsp://example.com/fizzle/foo RTSP/1.0
CSeq: 892
Session: 12345678

S->C: 
RTSP/1.0 200 OK
CSeq: 892
*/

#include "rtsp-client-internal.h"

static const char* sc_format =
		"TEARDOWN %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %s\r\n"
		"%s" // Authorization: Digest xxx
		"User-Agent: %s\r\n"
		"\r\n";

static int rtsp_client_media_teardown(struct rtsp_client_t* rtsp, int i)
{
	int r;
	assert(i < rtsp->media_count);
	assert(RTSP_TEARDWON == rtsp->state);
	if (i >= rtsp->media_count) return -1;

	assert(rtsp->media[i].uri[0] && rtsp->session[i].session[0]);
	r = rtsp_client_authenrization(rtsp, "TEARDOWN", rtsp->media[i].uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
	r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->media[i].uri, rtsp->cseq++, rtsp->session[i].session, rtsp->authenrization, USER_AGENT);
	assert(r > 0 && r < sizeof(rtsp->req));
	return r = rtsp->handler.send(rtsp->param, rtsp->media[i].uri, rtsp->req, r) ? 0 : -1;
}

int rtsp_client_teardown(struct rtsp_client_t* rtsp)
{
	int r;
	rtsp->state = RTSP_TEARDWON;
	rtsp->progress = 0;
	if (rtsp->aggregate)
	{
		assert(rtsp->media_count > 0);
		assert(rtsp->aggregate_uri[0]);
		r = rtsp_client_authenrization(rtsp, "TEARDOWN", rtsp->aggregate_uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
		r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->aggregate_uri, rtsp->cseq++, rtsp->session[0].session, rtsp->authenrization, USER_AGENT);
		assert(r > 0 && r < sizeof(rtsp->req));
		return r == rtsp->handler.send(rtsp->param, rtsp->aggregate_uri, rtsp->req, r) ? 0 : -1;
	}
	else
	{
		return rtsp_client_media_teardown(rtsp, rtsp->progress);
	}
}

static int rtsp_client_media_teardown_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int code;
	assert(RTSP_TEARDWON == rtsp->state);
	assert(rtsp->progress <= rtsp->media_count);

	code = http_get_status_code(parser);
	if (200 != code)
		return -1;
	
	if (rtsp->media_count == ++rtsp->progress)
	{
		return rtsp->handler.onteardown(rtsp->param);
	}
	else
	{
		// teardown next media
		return rtsp_client_media_teardown(rtsp, rtsp->progress);
	}
}

// aggregate control reply
static int rtsp_client_aggregate_teardown_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int code;
	assert(RTSP_TEARDWON == rtsp->state);
	assert(0 == rtsp->progress);
	assert(rtsp->aggregate);

	code = http_get_status_code(parser);
	if (459 == code) // 459 Aggregate operation not allowed (p26)
	{
		rtsp->aggregate = 0;
		return rtsp_client_media_teardown(rtsp, rtsp->progress);
	}
	else if (200 == code)
	{
		return rtsp->handler.onteardown(rtsp->param);
	}
	else
	{
		return -1;
	}
}

int rtsp_client_teardown_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	assert(RTSP_TEARDWON == rtsp->state);
	assert(rtsp->progress < rtsp->media_count);

	if (rtsp->aggregate)
		return rtsp_client_aggregate_teardown_onreply(rtsp, parser);
	else
		return rtsp_client_media_teardown_onreply(rtsp, parser);
}
