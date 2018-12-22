#include "rtsp-server-internal.h"
#include "rtsp-header-transport.h"
#include "rtsp-header-session.h"
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

int rtsp_server_setup(struct rtsp_server_t* rtsp, const char* uri)
{
	size_t n;
	const char *ptransport;
	struct rtsp_header_transport_t transport[16];

	memset(transport, 0, sizeof(transport));
	n = sizeof(transport) / sizeof(transport[0]);

	ptransport = http_get_header_by_name(rtsp->parser, "Transport");
	if (!ptransport || 0 != rtsp_header_transport_ex(ptransport, transport, &n) || 0 == n)
	{
		// 461 Unsupported Transport
		return rtsp_server_reply(rtsp, 461);
	}

	assert(n > 0);
	return rtsp->handler.onsetup(rtsp->param, rtsp, uri, rtsp->session.session[0] ? rtsp->session.session : NULL, transport, n);
}

int rtsp_server_reply_setup(struct rtsp_server_t *rtsp, int code, const char* sessionid, const char* transport)
{
	int n;
	char header[1024];

	// save session-id
	n = snprintf(rtsp->session.session, sizeof(rtsp->session.session), "%s", sessionid ? sessionid : "");
	if (n < 0 || n >= sizeof(rtsp->session.session))
	{
		assert(0); // sessionid too long
		return -1;
	}

	// RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257
	n = snprintf(header, sizeof(header), "Transport: %s\r\n", transport ? transport : "");
	if (n < 0 || n >= sizeof(header))
	{
		assert(0); // transport or sessionid too long
		return -1;
	}

	return rtsp_server_reply2(rtsp, code, header, NULL, 0);
}
