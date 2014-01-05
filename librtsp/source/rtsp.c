#include "rtsp.h"
#include "rtsp-parser.h"

int rtsp_options(void* rtsp)
{
}

int rtsp_describe(void* rtsp)
{
	const char* path;
	path = rtsp_get_request_uri(rtsp);
}
