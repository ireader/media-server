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

static const char* sc_rtsp_tcp = "SETUP %s RTSP/1.0\r\n"
								"CSeq: %u\r\n"
								"%s" // Session: %s\r\n
								"%s" // Authorization: Digest xxx
								"Transport: RTP/AVP/TCP;interleaved=%u-%u\r\n"
								"User-Agent: %s\r\n"
								"\r\n";

static const char* sc_rtsp_udp = "SETUP %s RTSP/1.0\r\n"
								"CSeq: %u\r\n"
								"%s" // Session: %s\r\n
								"%s" // Authorization: Digest xxx
								"Transport: RTP/AVP;unicast;client_port=%u-%u\r\n"
								"User-Agent: %s\r\n"
								"\r\n";

static int rtsp_client_media_setup(struct rtsp_client_t* rtsp)
{
	int len;
	struct rtsp_media_t *media;
	char session[sizeof(media->session.session) + 12], *p;

	assert(RTSP_SETUP == rtsp->state);
	assert(rtsp->progress < rtsp->media_count);
	media = rtsp_get_media(rtsp, rtsp->progress);
	if (NULL == media) return -1;

	p = rtsp->media[0].session.session;
	len = snprintf(session, sizeof(session), (rtsp->aggregate && *p) ? "Session: %s\r\n" : "", p);
	assert(len >= 0 && len < sizeof(session));

	// TODO: multicast
	assert(0 == media->transport.multicast);
	len = rtsp_client_authenrization(rtsp, "SETUP", media->uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
	len = snprintf(rtsp->req, sizeof(rtsp->req),
			RTSP_TRANSPORT_RTP_TCP==media->transport.transport?sc_rtsp_tcp:sc_rtsp_udp,
			media->uri, rtsp->cseq++, session, rtsp->authenrization, media->transport.rtp.u.client_port1, media->transport.rtp.u.client_port2, USER_AGENT);

	return len == rtsp->handler.send(rtsp->param, media->uri, rtsp->req, len) ? 0 : -1;
}

int rtsp_client_setup(struct rtsp_client_t* rtsp, const char* sdp)
{
	int i, r;
	struct rtsp_media_t *media;

	if (NULL == sdp || 0 == *sdp)
		return -1;

	r = rtsp_client_sdp(rtsp, sdp);
	if (0 != r)
		return r;

	for (i = 0; i < rtsp->media_count; i++)
	{
		media = rtsp_get_media(rtsp, i);
		r = rtsp->handler.rtpport(rtsp->param, &media->transport.rtp.u.client_port1);
		if (0 != r)
			return r;

		if (0 == media->transport.rtp.u.client_port1)
		{
			media->transport.transport = RTSP_TRANSPORT_RTP_TCP;
			media->transport.rtp.u.client_port1 = 2 * (unsigned short)i;
			media->transport.rtp.u.client_port2 = 2 * (unsigned short)i + 1;
		}
		else
		{
			assert(0 == media->transport.rtp.u.client_port1 % 2);
			media->transport.transport = RTSP_TRANSPORT_RTP_UDP;
			media->transport.rtp.u.client_port2 = media->transport.rtp.u.client_port1 + 1;
		}
	}

	rtsp->state = RTSP_SETUP;
	rtsp->progress = 0;
	return rtsp_client_media_setup(rtsp);
}

int rtsp_client_setup_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int code;
	const char *session;
	const char *transport;

	assert(RTSP_SETUP == rtsp->state);
	assert(rtsp->progress < rtsp->media_count);

	code = rtsp_get_status_code(parser);
	if (200 == code)
	{
		session = rtsp_get_header_by_name(parser, "Session");
		transport = rtsp_get_header_by_name(parser, "Transport");
		if (!session || 0 != rtsp_header_session(session, &rtsp->media[rtsp->progress].session)
			|| !transport || 0 != rtsp_header_transport(transport, &rtsp->media[rtsp->progress].transport))
		{
			printf("Get rtsp transport error.\n");
			return -EINVAL;
		}

		assert(strlen(session) < sizeof(rtsp->media[0].session.session));
		assert(!rtsp->aggregate || 0 == strcmp(rtsp->media[0].session.session, rtsp->media[rtsp->progress].session.session));

		if (rtsp->media_count == ++rtsp->progress)
		{
			return rtsp->handler.onsetup(rtsp->param);
		}
		else
		{
			// setup next media
			return rtsp_client_media_setup(rtsp);
		}
	}
	else if (401 == code)
	{
		// Unauthorized
		const char* authenticate;
		authenticate = rtsp_get_header_by_name(parser, "WWW-Authenticate");
		if (authenticate)
		{
			rtsp_client_www_authenticate(rtsp, authenticate);
		}
		return -EACCES; // try again
	}
	else
	{
		return -1;
	}
}
