#include "rtsp-server-internal.h"

// RFC 2326 10.1 OPTIONS (p30)
void rtsp_server_options(struct rtsp_session_t* session, const char* uri)
{
	static const char* methods = "DESCRIBE,SETUP,TEARDOWN,PLAY,PAUSE";
	int len;

	//	assert(0 == strcmp("*", uri));
	len = snprintf(session->reply, sizeof(session->reply),
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Public: %s\r\n"
		"\r\n",
		session->cseq, methods);

	session->send(session, session->reply, len);
}
