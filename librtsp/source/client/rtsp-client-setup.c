/*
C->S:
SETUP rtsp://example.com/foo/bar/baz.rm RTSP/1.0
CSeq: 302
Transport: RTP/AVP;unicast;client_port=4588-4589

S->C: 
RTSP/1.0 200 OK
CSeq: 302
Date: 23 Jan 1997 15:35:06 GMT
Session: 47112344
Transport: RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257
*/
#include "rtsp-client-internal.h"

static const char* sc_rtsp_setup = "SETUP %s RTSP/1.0\r\n"
								"CSeq: %u\r\n"
								"%s" // Session: %s\r\n
								"%s" // Authorization: Digest xxx
								"%s" // Transport: RTP/AVP;unicast;client_port=6000-6001
								"User-Agent: %s\r\n"
								"\r\n";

int rtsp_addr_is_multicast(const char* ip);

static int rtsp_client_media_setup(struct rtsp_client_t* rtsp, int i)
{
	int len;
    char transport[128];
	char session[sizeof(rtsp->session[0].session) + 12], *p;

	assert(i < rtsp->media_count);
	assert(RTSP_SETUP == rtsp->state);
	if (i >= rtsp->media_count) return -1;

	transport[0] = session[0] = '\0';
	p = rtsp->session[0].session;
    len = (rtsp->aggregate && *p) ? snprintf(session, sizeof(session), "Session: %s\r\n", p) : 0;
	assert(len >= 0 && len < sizeof(session));

    switch (rtsp->transport[i].transport) {
    case RTSP_TRANSPORT_RTP_TCP:
        len = snprintf(transport, sizeof(transport), "Transport: RTP/AVP/TCP;interleaved=%d-%d\r\n", rtsp->transport[i].interleaved1, rtsp->transport[i].interleaved2);
        break;
            
    case RTSP_TRANSPORT_RTP_UDP:
        if(0 == rtsp->transport[i].multicast)
            len = snprintf(transport, sizeof(transport), "Transport: RTP/AVP;unicast;client_port=%hu-%hu\r\n", rtsp->transport[i].rtp.u.client_port1, rtsp->transport[i].rtp.u.client_port2);
        else if(*rtsp->transport[i].destination)
            len = snprintf(transport, sizeof(transport), "Transport: RTP/AVP;multicast;destination=%s;port=%hu-%hu;ttl=%d\r\n", rtsp->transport[i].destination, rtsp->transport[i].rtp.m.port1, rtsp->transport[i].rtp.m.port2, rtsp->transport[i].rtp.m.ttl);
        else
            len = snprintf(transport, sizeof(transport), "Transport: RTP/AVP;multicast;port=%hu-%hu;ttl=%d\r\n", rtsp->transport[i].rtp.m.port1, rtsp->transport[i].rtp.m.port2, rtsp->transport[i].rtp.m.ttl);
        break;

    case RTSP_TRANSPORT_RAW:
        if(0 == rtsp->transport[i].multicast)
            len = snprintf(transport, sizeof(transport), "Transport: RAW/RAW/UDP;unicast;client_port=%hu-%hu\r\n", rtsp->transport[i].rtp.u.client_port1, rtsp->transport[i].rtp.u.client_port2);
        else if(*rtsp->transport[i].destination)
            len = snprintf(transport, sizeof(transport), "Transport: RAW/RAW/UDP;multicast;destination=%s;port=%hu-%hu;ttl=%d\r\n", rtsp->transport[i].destination, rtsp->transport[i].rtp.m.port1, rtsp->transport[i].rtp.m.port2, rtsp->transport[i].rtp.m.ttl);
        else
            len = snprintf(transport, sizeof(transport), "Transport: RAW/RAW/UDP;multicast;port=%hu-%hu;ttl=%d\r\n", rtsp->transport[i].rtp.m.port1, rtsp->transport[i].rtp.m.port2, rtsp->transport[i].rtp.m.ttl);
        break;

    default:
        assert(0);
        return -1;
    }
    assert(len >= 0 && len < sizeof(transport));
    
	len = rtsp_client_authenrization(rtsp, "SETUP", rtsp->media[i].uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
    len = snprintf(rtsp->req, sizeof(rtsp->req), sc_rtsp_setup, rtsp->media[i].uri, rtsp->cseq++, session, rtsp->authenrization, transport, USER_AGENT);
	return (len > 0 && len < sizeof(rtsp->req) && len == rtsp->handler.send(rtsp->param, rtsp->media[i].uri, rtsp->req, len)) ? 0 : -1;
}

int rtsp_client_setup(struct rtsp_client_t* rtsp, const char* sdp, int len)
{
	int i, r;
    unsigned short port[2];
    struct rtsp_media_t *m;
	struct rtsp_header_transport_t *t;

	if (NULL == sdp || 0 == *sdp || len < 1)
		return -1;

	r = rtsp_media_sdp(sdp, len, rtsp->media, sizeof(rtsp->media)/sizeof(rtsp->media[0]));
	if (r < 0 || r > sizeof(rtsp->media) / sizeof(rtsp->media[0]))
		return r < 0 ? r : -E2BIG; // too many media stream

	rtsp->media_count = r;
	for (i = 0; i < rtsp->media_count; i++)
	{
        m = rtsp->media + i;
        t = rtsp->transport + i;

		// rfc 2326 C.1.1 Control URL (p80)
		// If found at the session level, the attribute indicates the URL for aggregate control
		rtsp_media_set_url(m, rtsp->baseuri, rtsp->location, rtsp->uri);

        port[0] = (unsigned short)m->port[0];
        port[1] = (unsigned short)m->port[1];
        snprintf(t->source, sizeof(t->source), "%s", m->source);
        snprintf(t->destination, sizeof(t->destination), "%s", m->address);
        r = rtsp->handler.rtpport(rtsp->param, i, t->source, port, t->destination, sizeof(t->destination));
		if (r < 0)
			return r;

        if(RTSP_TRANSPORT_RTP_TCP == r)
		{
			t->transport = RTSP_TRANSPORT_RTP_TCP;
            t->interleaved1 = 0==port[0] ? 2 * (unsigned short)i : port[0];
            t->interleaved2 = 0==port[1] ? t->interleaved1 + 1 : port[1];
		}
		else if((RTSP_TRANSPORT_RTP_UDP == r || RTSP_TRANSPORT_RAW == r) && rtsp_addr_is_multicast(t->destination))
		{
			assert(0 == t->rtp.u.client_port1 % 2);
            t->transport = r;
            t->multicast = 1;
            t->rtp.m.ttl = 16; // default RTT
			t->rtp.m.port1 = port[0];
            t->rtp.m.port2 = 0 == port[1] ? t->rtp.m.port1 + 1 : port[1];
		}
        else if(RTSP_TRANSPORT_RTP_UDP == r || RTSP_TRANSPORT_RAW == r)
        {
            assert(0 == t->rtp.u.client_port1 % 2);
            t->transport = r;
            t->multicast = 0;
            t->rtp.u.client_port1 = port[0];
            t->rtp.u.client_port2 = 0 == port[1] ? t->rtp.u.client_port1 + 1 : port[1];
        }
		else if (0 == r)
		{
			// ignore media
			if (i + 1 < rtsp->media_count)
				memmove(rtsp->media + i, rtsp->media + i + 1, sizeof(rtsp->media[0]) * (rtsp->media_count - i - 1));
			rtsp->media_count--;
			i--; // redo
			continue;
		}
        else
        {
            assert(0);
            return -1;
        }
	}

	rtsp->aggregate = (rtsp->media_count > 1 && rtsp->media[0].session_uri[0]) ? 1 : 0;
	if (rtsp->aggregate)
	{
		snprintf(rtsp->aggregate_uri, sizeof(rtsp->aggregate_uri), "%s", rtsp->media[0].session_uri);
	}
	else if(rtsp->media_count > 1)
	{
		// fix some IPC set Content-Base only
		const char* base;
		base = rtsp->baseuri[0] ? rtsp->baseuri : rtsp->location;
		rtsp->aggregate = base[0] ? 1 : 0;
		if(rtsp->aggregate)
			snprintf(rtsp->aggregate_uri, sizeof(rtsp->aggregate_uri), "%s", base);
	}

	rtsp->state = RTSP_SETUP;
	rtsp->progress = 0;
	return rtsp_client_media_setup(rtsp, rtsp->progress);
}

int rtsp_client_setup_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int code;
	const char *session;
	const char *transport;
	struct rtsp_header_range_t* range;

	assert(RTSP_SETUP == rtsp->state);
	assert(rtsp->progress < rtsp->media_count);

	code = http_get_status_code(parser);
	if (200 == code)
	{
		session = http_get_header_by_name(parser, "Session");
		transport = http_get_header_by_name(parser, "Transport");
		if (!session || 0 != rtsp_header_session(session, &rtsp->session[rtsp->progress])
			|| !transport || 0 != rtsp_header_transport(transport, &rtsp->transport[rtsp->progress]))
		{
			printf("Get rtsp transport error.\n");
			return -EINVAL;
		}

		//assert(rtsp->media[rtsp->progress].transport.transport != RTSP_TRANSPORT_RTP_TCP || (rtsp->media[rtsp->progress].transport.rtp.u.client_port1== rtsp->media[rtsp->progress].transport.interleaved1 && rtsp->media[rtsp->progress].transport.rtp.u.client_port2 == rtsp->media[rtsp->progress].transport.interleaved2));
		assert(strlen(session) < sizeof(rtsp->session[0].session));
		assert(!rtsp->aggregate || 0 == strcmp(rtsp->session[0].session, rtsp->session[rtsp->progress].session));

		if (rtsp->media_count == ++rtsp->progress)
		{
			assert(rtsp->media_count > 0);
			range = &rtsp->media[0].range;
			return rtsp->handler.onsetup(rtsp->param, rtsp->session[rtsp->progress].timeout / 1000, RTSP_RANGE_TIME_NORMAL==range->from_value && RTSP_RANGE_TIME_NORMAL==range->to_value ? range->to - range->from : -1);
		}
		else
		{
			// setup next media
			return rtsp_client_media_setup(rtsp, rtsp->progress);
		}
	}
	else if (401 == code)
	{
		// Unauthorized
		const char* authenticate;
		authenticate = http_get_header_by_name(parser, "WWW-Authenticate");
		if (authenticate)
		{
			rtsp_client_www_authenticate(rtsp, authenticate);
		}
		return -EACCES; // try again
	}
	else if (461 == code)
	{
		// Unsupported Transport
		return -1;
	}
	else
	{
		return -1;
	}
}
