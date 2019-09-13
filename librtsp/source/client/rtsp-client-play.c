// RFC-2326 10.5 PLAY (p34)
// 1. A PLAY request without a Range header is legal. It starts playing a
//    stream from the beginning unless the stream has been paused. If a
//	  stream has been paused via PAUSE, stream delivery resumes at the pause point.
// 2. If a stream is playing, such a PLAY request causes no
//    further action and can be used by the client to test server liveness.

/*
PLAY rtsp://audio.example.com/audio RTSP/1.0
CSeq: 835
Session: 12345678
Range: npt=10-15

C->S: 
PLAY rtsp://audio.example.com/twister.en RTSP/1.0
CSeq: 833
Session: 12345678
Range: smpte=0:10:20-;time=19970123T153600Z

S->C: 
RTSP/1.0 200 OK
CSeq: 833
Date: 23 Jan 1997 15:35:06 GMT
Range: smpte=0:10:22-;time=19970123T153600Z

C->S: 
PLAY rtsp://audio.example.com/meeting.en RTSP/1.0
CSeq: 835
Session: 12345678
Range: clock=19961108T142300Z-19961108T143520Z

S->C: 
RTSP/1.0 200 OK
CSeq: 835
Date: 23 Jan 1997 15:35:06 GMT
*/

#include "rtsp-client.h"
#include "rtsp-client-internal.h"
#include "rtsp-header-range.h"
#include "rtsp-header-rtp-info.h"
#include <assert.h>

static const char* sc_format = 
	"PLAY %s RTSP/1.0\r\n"
	"CSeq: %u\r\n"
	"Session: %s\r\n"
	"%s" // Range
	"%s" // Speed
	"%s" // Authorization: Digest xxx
	"User-Agent: %s\r\n"
	"\r\n";

static int rtsp_client_media_play(struct rtsp_client_t *rtsp, int i)
{
	int r;
	assert(0 == rtsp->aggregate);
	assert(i < rtsp->media_count);
	assert(RTSP_PLAY == rtsp->state);
	if (i >= rtsp->media_count) return -1;

	assert(rtsp->media[i].uri[0] && rtsp->session[i].session[0]);
	r = rtsp_client_authenrization(rtsp, "PLAY", rtsp->media[i].uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
	r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->media[i].uri, rtsp->cseq++, rtsp->session[i].session, rtsp->range, rtsp->speed, rtsp->authenrization, USER_AGENT);
	assert(r > 0 && r < sizeof(rtsp->req));
	return r == rtsp->handler.send(rtsp->param, rtsp->media[i].uri, rtsp->req, r) ? 0 : -1;
}

int rtsp_client_play(struct rtsp_client_t *rtsp, const uint64_t *npt, const float *speed)
{
	int r;
	assert(RTSP_SETUP == rtsp->state || RTSP_PLAY == rtsp->state || RTSP_PAUSE == rtsp->state);
	rtsp->state = RTSP_PLAY;
	rtsp->progress = 0;

	r = snprintf(rtsp->speed, sizeof(rtsp->speed), speed ? "Speed: %f\r\n" : "", speed ? *speed : 0.0f);
	r = snprintf(rtsp->range, sizeof(rtsp->range), npt ? "Range: npt=%" PRIu64 ".%" PRIu64 "-\r\n" : "", npt ? *npt/1000 : 0, npt ? *npt%1000 : 0);
	if (r < 0 || r >= sizeof(rtsp->range))
		return -1;
	
	if(rtsp->aggregate)
	{
		assert(rtsp->media_count > 0);
		assert(rtsp->aggregate_uri[0]);
		r = rtsp_client_authenrization(rtsp, "PLAY", rtsp->aggregate_uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
		r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->aggregate_uri, rtsp->cseq++, rtsp->session[0].session, rtsp->range, rtsp->speed, rtsp->authenrization, USER_AGENT);
		assert(r > 0 && r < sizeof(rtsp->req));
		return r == rtsp->handler.send(rtsp->param, rtsp->aggregate_uri, rtsp->req, r) ? 0 : -1;
	}
	else
	{
		return rtsp_client_media_play(rtsp, rtsp->progress);
	}
}

static int rtsp_client_media_play_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int i, r;
	uint64_t npt0 = (uint64_t)(-1);
	uint64_t npt1 = (uint64_t)(-1);
	double scale = 0.0f;
	const char *prange, *pscale, *prtpinfo, *pnext;
	struct rtsp_header_range_t range;
	struct rtsp_header_rtp_info_t rtpinfo[N_MEDIA];
	struct rtsp_rtp_info_t rtpInfo[N_MEDIA];

	if (200 != http_get_status_code(parser))
		return -1;

	prange = http_get_header_by_name(parser, "Range");
	pscale = http_get_header_by_name(parser, "Scale");
	prtpinfo = http_get_header_by_name(parser, "RTP-Info");

	if (pscale)
	{
		scale = atof(pscale);
	}

	if (prange && 0 == rtsp_header_range(prange, &range))
	{
		assert(range.from_value == RTSP_RANGE_TIME_NORMAL);
		assert(range.to_value != RTSP_RANGE_TIME_NOW);
		npt0 = range.from;
		npt1 = range.to_value == RTSP_RANGE_TIME_NOVALUE ? -1 : range.to;
	}

	memset(rtpInfo, 0, sizeof(rtpInfo));
	for (i = 0; prtpinfo && i < sizeof(rtpInfo) / sizeof(rtpInfo[0]); i++)
	{
		prtpinfo += strspn(prtpinfo, " "); // skip space
		pnext = strchr(prtpinfo, ',');
		if (0 == rtsp_header_rtp_info(prtpinfo, &rtpinfo[i]))
		{
			rtpInfo[i].uri = rtpinfo[i].url;
			rtpInfo[i].seq = (unsigned int)rtpinfo[i].seq;
			rtpInfo[i].time = (unsigned int)rtpinfo[i].rtptime;
		}
		prtpinfo = pnext ? pnext + 1 : pnext;
	}

	r = rtsp->handler.onplay(rtsp->param, rtsp->progress, (uint64_t)(-1) == npt0 ? NULL : &npt0, (uint64_t)(-1) == npt1 ? NULL : &npt1, pscale ? &scale : NULL, rtpInfo, i);

	if(0 == r && 0 == rtsp->aggregate && rtsp->media_count > ++rtsp->progress)
	{
		return rtsp_client_media_play(rtsp, rtsp->progress);
	}
	return r;
}

// aggregate control reply
static int rtsp_client_aggregate_play_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int code;
	assert(RTSP_PLAY == rtsp->state);
	assert(0 == rtsp->progress);
	assert(rtsp->aggregate);
	
	code = http_get_status_code(parser);
	if (459 == code) // 459 Aggregate operation not allowed (p26)
	{
		rtsp->aggregate = 0;
		return rtsp_client_media_play(rtsp, rtsp->progress);
	}
	else if (200 == code)
	{
		return rtsp_client_media_play_onreply(rtsp, parser);
	}
	else
	{
		return -1;
	}
}

int rtsp_client_play_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	assert(RTSP_PLAY == rtsp->state);
	assert(rtsp->progress < rtsp->media_count);

	if (rtsp->aggregate)
		return rtsp_client_aggregate_play_onreply(rtsp, parser);
	else
		return rtsp_client_media_play_onreply(rtsp, parser);
}
