#ifndef _rtsp_url_parser_h_
#define _rtsp_url_parser_h_

#include "url.h"
#include "cstringext.h"

// 3.2 RTSP URL (p14)
enum { 
	RTSP_SCHEME_TCP = 1, // rtsp
	RTSP_SCHEME_UDP,	 // rtspu
};

static inline int rtsp_url_parse(const char* uri, int *scheme, char *ip, int bytes, int *port)
{
	void* parser;
	const char* transport;
	const char* host;

	parser = url_parse(uri);
	if(!parser) return -1;

	transport = url_getscheme(parser);
	if(transport && 0 == strcasecmp("rtspu", transport))
		*scheme = RTSP_SCHEME_UDP;
	else
		*scheme = RTSP_SCHEME_TCP;

	host = url_gethost(parser);
	if(!host)
	{
		url_free(parser);
		return -1;
	}

	strlcpy(ip, host, bytes);

	*port = url_getport(parser);
	if(0 == *port)
		*port = 554; // default

	url_free(parser);
	return 0;
}

#endif /* !_rtsp_url_parser_h_ */
