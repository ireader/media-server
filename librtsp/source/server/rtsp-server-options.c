#include "rtsp-server-internal.h"

// RFC 2326 10.1 OPTIONS (p30)
int rtsp_server_options(struct rtsp_server_t* rtsp, const char* uri)
{
	static const char* methods = "DESCRIBE,SETUP,TEARDOWN,PLAY,PAUSE";
	int len;
	(void)uri;

	//	assert(0 == strcmp("*", uri));
	len = snprintf(rtsp->reply, sizeof(rtsp->reply),
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Public: %s\r\n"
		"\r\n",
		rtsp->cseq, methods);

	return rtsp->handler.send(rtsp->sendparam, rtsp->reply, len);
}
