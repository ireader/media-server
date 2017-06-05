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
		"User-Agent: %s\r\n"
		"\r\n";

static int rtsp_client_media_teardown(struct rtsp_client_t* rtsp)
{
	int r;
	struct rtsp_media_t* media;

	assert(RTSP_TEARDWON == rtsp->state);
	assert(rtsp->progress < rtsp->media_count);
	media = rtsp_get_media(rtsp, rtsp->progress);
	if (NULL == media) return -1;

	assert(media->uri[0] && media->session.session[0]);
	r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, media->uri, rtsp->cseq++, media->session.session, USER_AGENT);
	assert(r > 0 && r < sizeof(rtsp->req));
	return r = rtsp->handler.send(rtsp->param, media->uri, rtsp->req, r) ? 0 : -1;
}

int rtsp_client_teardown(struct rtsp_client_t* rtsp)
{
	rtsp->state = RTSP_TEARDWON;
	rtsp->progress = 0;
	return rtsp_client_media_teardown(rtsp);
}

int rtsp_client_teardown_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int code;
	assert(RTSP_TEARDWON == rtsp->state);
	assert(rtsp->progress <= rtsp->media_count);

	code = rtsp_get_status_code(parser);
	if (200 != code)
		return -1;
	
	if (rtsp->media_count == ++rtsp->progress)
	{
		rtsp->handler.onclose(rtsp->param);
		return 0;
	}
	else
	{
		// teardown next media
		return rtsp_client_media_teardown(rtsp);
	}
}
