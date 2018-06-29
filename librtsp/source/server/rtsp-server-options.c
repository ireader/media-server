#include "rtsp-server-internal.h"

// RFC 2326 10.1 OPTIONS (p30)
int rtsp_server_options(struct rtsp_server_t* rtsp, const char* uri)
{
	(void)uri;
	return rtsp_server_reply2(rtsp, 200, "Public: DESCRIBE,SETUP,TEARDOWN,PLAY,PAUSE,ANNOUNCE,RECORD\r\n");
}
