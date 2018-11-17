// RFC-2326 10.6 PAUSE (p36)
// 1. A PAUSE request discards all queued PLAY requests. However, the pause
//    point in the media stream MUST be maintained. A subsequent PLAY
//    request without Range header resumes from the pause point. (p36) 
// 2. The PAUSE request may contain a Range header specifying when the
//    stream or presentation is to be halted. (p36) (p45 for more)

/*
C->S: 
PAUSE rtsp://example.com/fizzle/foo RTSP/1.0
CSeq: 834
Session: 12345678

S->C: 
RTSP/1.0 200 OK
CSeq: 834
Date: 23 Jan 1997 15:35:06 GMT
*/

#include "rtsp-client.h"
#include "rtsp-client-internal.h"

static const char* sc_format = 
	"PAUSE %s RTSP/1.0\r\n"
	"CSeq: %u\r\n"
	"Session: %s\r\n"
	"%s" // Authorization: Digest xxx
	"User-Agent: %s\r\n"
	"\r\n";

static int rtsp_client_media_pause(struct rtsp_client_t *rtsp, int i)
{
	int r;
	assert(0 == rtsp->aggregate);
	assert(i < rtsp->media_count);
	assert(RTSP_PAUSE == rtsp->state);
	if (i >= rtsp->media_count) return -1;

	assert(rtsp->media[i].uri[0] && rtsp->session[i].session[0]);
	r = rtsp_client_authenrization(rtsp, "PAUSE", rtsp->media[i].uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
	r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->media[i].uri, rtsp->cseq++, rtsp->session[i].session, rtsp->authenrization, USER_AGENT);
	assert(r > 0 && r < sizeof(rtsp->req));
	return r == rtsp->handler.send(rtsp->param, rtsp->media[i].uri, rtsp->req, r) ? 0 : -1;
}

int rtsp_client_pause(struct rtsp_client_t *rtsp)
{
	int r;
	assert(RTSP_SETUP == rtsp->state || RTSP_PLAY == rtsp->state || RTSP_PAUSE == rtsp->state);
	rtsp->state = RTSP_PAUSE;
	rtsp->progress = 0;

	if(rtsp->aggregate)
	{
		assert(rtsp->media_count > 0);
		assert(rtsp->aggregate_uri[0]);
		r = rtsp_client_authenrization(rtsp, "PAUSE", rtsp->aggregate_uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
		r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->aggregate_uri, rtsp->cseq++, rtsp->session[0].session, rtsp->authenrization, USER_AGENT);
		assert(r > 0 && r < sizeof(rtsp->req));
		return r == rtsp->handler.send(rtsp->param, rtsp->aggregate_uri, rtsp->req, r) ? 0 : -1;
	}
	else
	{
		return rtsp_client_media_pause(rtsp, rtsp->progress);
	}
}

static int rtsp_client_media_pause_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int code;
	assert(0 == rtsp->aggregate);
	assert(rtsp->progress < rtsp->media_count);

	code = http_get_status_code(parser);
	assert(460 != code); // 460 Only aggregate operation allowed (p26)
	if (200 == code)
	{
		if (rtsp->media_count == ++rtsp->progress)
		{
			return rtsp->handler.onpause(rtsp->param);
		}
		else
		{
			return rtsp_client_media_pause(rtsp, rtsp->progress);
		}
	}
	else
	{
		return -1;
	}
}

// aggregate control reply
static int rtsp_client_aggregate_pause_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int code;

	assert(rtsp->aggregate);
	code = http_get_status_code(parser);
	if (459 == code) // 459 Aggregate operation not allowed (p26)
	{
		rtsp->aggregate = 0;
		return rtsp_client_media_pause(rtsp, rtsp->progress);
	}
	else if(200 == code)
	{
		return rtsp->handler.onpause(rtsp->param);
	}

	return -1;
}

int rtsp_client_pause_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	assert(RTSP_PAUSE == rtsp->state);
	assert(rtsp->progress < rtsp->media_count);

	if (rtsp->aggregate)
		return rtsp_client_aggregate_pause_onreply(rtsp, parser);
	else
		return rtsp_client_media_pause_onreply(rtsp, parser);
}
