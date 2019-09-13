// RFC-2326 14.6 Recording (p71)

/*
C->M: 
SETUP rtsp://server.example.com/meeting/videotrack RTSP/1.0
CSeq: 92
Session: 50887676
Transport: RTP/AVP;multicast;destination=224.0.1.12;port=61010-61011;mode=record;ttl=127

M->C: 
RTSP/1.0 200 OK
CSeq: 92
Transport: RTP/AVP;multicast;destination=224.0.1.12;port=61010-61011;mode=record;ttl=127

C->M: 
RECORD rtsp://server.example.com/meeting RTSP/1.0
CSeq: 93
Session: 50887676
Range: clock=19961110T1925-19961110T2015

M->C: 
RTSP/1.0 200 OK
CSeq: 93
*/

#include "rtsp-client.h"
#include "rtsp-client-internal.h"
#include "rtsp-header-range.h"
#include "rtsp-header-rtp-info.h"
#include <assert.h>

static const char* sc_format =
    "RECORD %s RTSP/1.0\r\n"
    "CSeq: %u\r\n"
    "Session: %s\r\n"
    "%s" // Range
    "%s" // Scale
    "%s" // Authorization: Digest xxx
    "User-Agent: %s\r\n"
    "\r\n";

static int rtsp_client_media_record(struct rtsp_client_t *rtsp, int i)
{
    int r;
    assert(0 == rtsp->aggregate);
	assert(i < rtsp->media_count);
    assert(RTSP_RECORD == rtsp->state);
	if (i >= rtsp->media_count) return -1;

	assert(rtsp->media[i].uri[0] && rtsp->session[i].session[0]);
    r = rtsp_client_authenrization(rtsp, "RECORD", rtsp->media[i].uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
    r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->media[i].uri, rtsp->cseq++, rtsp->session[i].session, rtsp->range, rtsp->scale, rtsp->authenrization, USER_AGENT);
    assert(r > 0 && r < sizeof(rtsp->req));
    return r == rtsp->handler.send(rtsp->param, rtsp->media[i].uri, rtsp->req, r) ? 0 : -1;
}

int rtsp_client_record(struct rtsp_client_t *rtsp, const uint64_t *npt, const float *scale)
{
    int r;
    assert(RTSP_SETUP == rtsp->state || RTSP_RECORD == rtsp->state || RTSP_PAUSE == rtsp->state);
    rtsp->state = RTSP_RECORD;
    rtsp->progress = 0;

	r = snprintf(rtsp->scale, sizeof(rtsp->scale), scale ? "Scale: %f\r\n" : "", scale ? *scale : 0.0f);
	r = snprintf(rtsp->range, sizeof(rtsp->range), npt ? "Range: npt=%" PRIu64 ".%" PRIu64 "-\r\n" : "", npt ? *npt / 1000 : 0, npt ? *npt % 1000 : 0);
    if (r < 0 || r >= sizeof(rtsp->range))
		return -1;

    if (rtsp->aggregate)
    {
        assert(rtsp->media_count > 0);
        assert(rtsp->aggregate_uri[0]);
        r = rtsp_client_authenrization(rtsp, "RECORD", rtsp->aggregate_uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
        r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->aggregate_uri, rtsp->cseq++, rtsp->session[0].session, rtsp->range, rtsp->scale, rtsp->authenrization, USER_AGENT);
        assert(r > 0 && r < sizeof(rtsp->req));
        return r == rtsp->handler.send(rtsp->param, rtsp->aggregate_uri, rtsp->req, r) ? 0 : -1;
    }
    else
    {
        return rtsp_client_media_record(rtsp, rtsp->progress);
    }
}

static int rtsp_client_media_record_onreply(struct rtsp_client_t* rtsp, void* parser)
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

    r = rtsp->handler.onrecord(rtsp->param, rtsp->progress, (uint64_t)(-1) == npt0 ? NULL : &npt0, (uint64_t)(-1) == npt1 ? NULL : &npt1, pscale ? &scale : NULL, rtpInfo, i);

    if (0 == r && 0 == rtsp->aggregate && rtsp->media_count > ++rtsp->progress)
    {
        return rtsp_client_media_record(rtsp, rtsp->progress);
    }
    return r;
}

// aggregate control reply
static int rtsp_client_aggregate_record_onreply(struct rtsp_client_t* rtsp, void* parser)
{
    int code;
    assert(RTSP_RECORD == rtsp->state);
    assert(0 == rtsp->progress);
    assert(rtsp->aggregate);

    code = http_get_status_code(parser);
    if (459 == code) // 459 Aggregate operation not allowed (p26)
    {
        rtsp->aggregate = 0;
        return rtsp_client_media_record(rtsp, rtsp->progress);
    }
    else if (200 == code)
    {
        return rtsp_client_media_record_onreply(rtsp, parser);
    }
    else
    {
        return -1;
    }
}

int rtsp_client_record_onreply(struct rtsp_client_t* rtsp, void* parser)
{
    assert(RTSP_RECORD == rtsp->state);
    assert(rtsp->progress < rtsp->media_count);

    if (rtsp->aggregate)
        return rtsp_client_aggregate_record_onreply(rtsp, parser);
    else
        return rtsp_client_media_record_onreply(rtsp, parser);
}
