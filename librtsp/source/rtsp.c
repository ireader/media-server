#include "rtsp.h"
#include "rtsp-parser.h"

/*
C->S:
OPTIONS * RTSP/1.0
CSeq: 1
Require: implicit-play
Proxy-Require: gzipped-messages

S->C:
RTSP/1.0 200 OK
CSeq: 1
Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE

C->S: 
SETUP rtsp://server.com/foo/bar/baz.rm RTSP/1.0
CSeq: 302
Require: funky-feature
Funky-Parameter: funkystuff

S->C: 
RTSP/1.0 551 Option not supported
CSeq: 302
Unsupported: funky-feature
*/
int rtsp_options(void* rtsp)
{
}

int rtsp_describe(void* rtsp)
{
	const char* path;
	path = rtsp_get_request_uri(rtsp);
}

int rtsp_setup(void* rtsp)
{
}

/*
The response 454 (Session Not Found) is returned if the session
identifier is invalid.
*/
