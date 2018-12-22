#include "rtsp-server-internal.h"

int rtsp_server_handle(struct rtsp_server_t *rtsp)
{
	char protocol[8];
	int major, minor;
	const char* uri;
	const char* method;
	const char* session;

	http_get_version(rtsp->parser, protocol, &major, &minor);
	if (1 != major && 0 != minor)
	{
		//505 RTSP Version Not Supported
		return rtsp_server_reply(rtsp, 505);
	}

	if (0 != http_get_header_by_name2(rtsp->parser, "CSeq", (int*)&rtsp->cseq))
	{
		// 400 Bad Request
		return rtsp_server_reply(rtsp, 400);
	}

	// parse session
	rtsp->session.session[0] = 0; // clear session value
	session = http_get_header_by_name(rtsp->parser, "Session");
	if (session)
		rtsp_header_session(session, &rtsp->session);
	
	uri = http_get_request_uri(rtsp->parser);
	method = http_get_request_method(rtsp->parser);

	switch (*method)
	{
	case 'o':
	case 'O':
		if (0 == strcasecmp("OPTIONS", method))
			return rtsp_server_options(rtsp, uri);
		break;

	case 'd':
	case 'D':
		if (0 == strcasecmp("DESCRIBE", method) && rtsp->handler.ondescribe)
			return rtsp_server_describe(rtsp, uri);
		break;

	case 'g':
	case 'G':
		if (0 == strcasecmp("GET_PARAMETER", method) && rtsp->handler.ongetparameter)
			return rtsp_server_get_parameter(rtsp, uri);
		break;

	case 's':
	case 'S':
		if (0 == strcasecmp("SETUP", method) && rtsp->handler.onsetup)
			return rtsp_server_setup(rtsp, uri);
		else if (0 == strcasecmp("SET_PARAMETER", method) && rtsp->handler.onsetparameter)
			return rtsp_server_set_parameter(rtsp, uri);
		break;

	case 'p':
	case 'P':
		if (0 == strcasecmp("PLAY", method) && rtsp->handler.onplay)
			return rtsp_server_play(rtsp, uri);
		else if (0 == strcasecmp("PAUSE", method) && rtsp->handler.onpause)
			return rtsp_server_pause(rtsp, uri);
		break;

	case 't':
	case 'T':
		if (0 == strcasecmp("TEARDOWN", method) && rtsp->handler.onteardown)
			return rtsp_server_teardown(rtsp, uri);
		break;

    case 'a':
    case 'A':
        if (0 == strcasecmp("ANNOUNCE", method) && rtsp->handler.onannounce)
            return rtsp_server_announce(rtsp, uri);
        break;

    case 'r':
    case 'R':
        if (0 == strcasecmp("RECORD", method) && rtsp->handler.onrecord)
            return rtsp_server_record(rtsp, uri);
        break;
	}

	// 501 Not implemented
	return rtsp_server_reply(rtsp, 501);
}
