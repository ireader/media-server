#ifndef _rtmp_url_h_
#define _rtmp_url_h_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "urlcodec.h"
#include "uri-parse.h"

#define URL_LENGTH 256

struct rtmp_url_t
{
	unsigned short port; // default 1935
	char scheme[32];
	char host[URL_LENGTH];
	char app[URL_LENGTH];
	char stream[URL_LENGTH];
	char tcurl[URL_LENGTH];
};

static int rtmp_url_parse(const char* url, struct rtmp_url_t* u)
{
	struct uri_t* uri = uri_parse(url, strlen(url));
	if (!uri) return -1;
	u->port = 0 == uri->port ? 1935 : uri->port;
	snprintf(u->host, sizeof(u->host), "%s", uri->host);
	snprintf(u->scheme, sizeof(u->scheme), "%s", uri->scheme);
	uri_free(uri);

	const char* p = strstr(url, "://");
	p = p ? p + 3 : url;
	p = strchr(p, '/');
	if (!p) return -1;
	p += 1;

	const char* p1 = strchr(p, '/');
	if (!p1) return -1;

	if (p + sizeof(u->app) - 1 < p1 || strlen(p1+1) + 1 > sizeof(u->stream) )
		return -1;
	url_decode(p, p1 - p, u->app, sizeof(u->app));
	url_decode(p1+1, strlen(p1+1), u->stream, sizeof(u->stream));
	snprintf(u->tcurl, sizeof(u->tcurl), "rtmp://%s:%d/%s", u->host, u->port, u->app);
	return 0;
}

#endif /* !_rtmp_url_h_ */
