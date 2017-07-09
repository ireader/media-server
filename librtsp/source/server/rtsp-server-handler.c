#include "rtsp-server-internal.h"

#if defined(OS_WINDOWS) || defined(_WIN32) || defined(_WIN64)
#define strcasecmp _stricmp
#endif

void rtsp_server_handle(struct rtsp_session_t *session)
{
	int major, minor;
	const char* uri;
	const char* method;

	rtsp_get_version(session->parser, &major, &minor);
	if (1 != major && 0 != minor)
	{
		//505 RTSP Version Not Supported
		rtsp_server_reply(session, 505);
		return;
	}

	if (0 != rtsp_get_header_by_name2(session->parser, "CSeq", (int*)&session->cseq))
	{
		// 400 Bad Request
		rtsp_server_reply(session, 400);
		return;
	}

	uri = rtsp_get_request_uri(session->parser);
	method = rtsp_get_request_method(session->parser);

	switch (*method)
	{
	case 'o':
	case 'O':
		if (0 == strcasecmp("OPTIONS", method))
		{
			rtsp_server_options(session, uri);
			return;
		}
		break;

	case 'd':
	case 'D':
		if (0 == strcasecmp("DESCRIBE", method))
		{
			rtsp_server_describe(session, uri);
			return;
		}
		break;

	case 's':
	case 'S':
		if (0 == strcasecmp("SETUP", method))
		{
			rtsp_server_setup(session, uri);
			return;
		}
		break;

	case 'p':
	case 'P':
		if (0 == strcasecmp("PLAY", method))
		{
			rtsp_server_play(session, uri);
			return;
		}
		else if (0 == strcasecmp("PAUSE", method))
		{
			rtsp_server_pause(session, uri);
			return;
		}
		break;

	case 't':
	case 'T':
		if (0 == strcasecmp("TEARDOWN", method))
		{
			rtsp_server_teardown(session, uri);
			return;
		}
		break;
	}

	// 501 Not implemented
	rtsp_server_reply(session, 501);
}
