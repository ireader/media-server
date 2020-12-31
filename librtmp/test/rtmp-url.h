#ifndef _rtmp_url_h_
#define _rtmp_url_h_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "urlcodec.h"
#include "uri-parse.h"

struct rtmp_url_t
{
	unsigned short port; // default 1935
	char* scheme;
	char* host;
	char* app;
	char* stream;
    char* tcurl;
    char __ptr[1024];
};

static int rtmp_url_parse(const char* url, struct rtmp_url_t* u)
{
    int r, n;
    const char* p;
    const char* p1;
    struct uri_t* uri;
    uri = uri_parse(url, (int)strlen(url));
	if (!uri) return -1;
    
    n = 0;
    memset(u, 0, sizeof(*u));
    u->port = 0 == uri->port ? 1935 : uri->port;
    
    u->host = u->__ptr + n;
    r = snprintf(u->host, sizeof(u->__ptr) - n, "%s", uri->host);
    if(r <= 0 || r >= sizeof(u->__ptr) - n)
        goto FAILED_PARSE_RTMP_URL;
    n += r + 1;
    
    u->scheme = u->__ptr + n;
	r = snprintf(u->scheme, sizeof(u->__ptr) - n, "%s", uri->scheme);
    if(r <= 0 || r >= sizeof(u->__ptr) - n)
        goto FAILED_PARSE_RTMP_URL;
    n += r + 1;
    
	uri_free(uri);
    
	p = strstr(url, "://");
	p = p ? p + 3 : url;
	p = strchr(p, '/');
	if (!p) return -1;
	p += 1;

	p1 = strchr(p, '/');
	if (!p1) return -1;

    u->app = u->__ptr + n;
	r = url_decode(p, (int)(p1 - p), u->app, sizeof(u->__ptr) - n);
    if(r <= 0 || r >= sizeof(u->__ptr) - n)
        return -1;
    n += r + 1;
    
    u->stream = u->__ptr + n;
	r = url_decode(p1+1, (int)strlen(p1+1), u->stream, sizeof(u->__ptr) - n);
    if(r <= 0 || r >= sizeof(u->__ptr) - n)
        return -1;
    n += r + 1;
    
    u->tcurl = u->__ptr + n;
	r = snprintf(u->tcurl, sizeof(u->__ptr) - n, "rtmp://%s:%d/%s", u->host, u->port, u->app);
    if(r <= 0 || r >= sizeof(u->__ptr) - n)
        return -1;
    n += r + 1;
    
    return 0;
    
FAILED_PARSE_RTMP_URL:
    uri_free(uri);
    return -1;
}

#endif /* !_rtmp_url_h_ */
