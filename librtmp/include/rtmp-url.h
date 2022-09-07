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
    char* vhost; // rtmp://host/vhost/app/stream or rtmp://host/app/stream?vhost=xxx
    char __ptr[1024];
};

static int rtmp_url_parse(const char* url, struct rtmp_url_t* u)
{
    int r, n;
    size_t i, j;
    char** v[3];
    const char* p[4];
    const char* pend[4];
    struct uri_t* uri;
    struct uri_query_t* q;
    static const int PORT = 1935;

    uri = uri_parse(url, (int)strlen(url));
	if (!uri) return -1;
    
    n = 0;
    memset(u, 0, sizeof(*u));
    u->port = (u_short)(0 == uri->port ? PORT : uri->port);
    
    u->host = u->__ptr + n;
    r = snprintf(u->host, sizeof(u->__ptr) - n, "%s", uri->host);
    if(r <= 0 || (size_t)r >= sizeof(u->__ptr) - n)
        goto FAILED_PARSE_RTMP_URL;
    n += r + 1;
    u->vhost = u->host; // default value
    
    u->scheme = u->__ptr + n;
	r = snprintf(u->scheme, sizeof(u->__ptr) - n, "%s", uri->scheme);
    if(r <= 0 || (size_t)r >= sizeof(u->__ptr) - n)
        goto FAILED_PARSE_RTMP_URL;
    n += r + 1;
    
    // parse vhost
    if (uri->query && *uri->query)
    {
        q = NULL;
        for(r = uri_query(uri->query, uri->query + strlen(uri->query), &q); r > 0; r--)
        {
            if (5 == q[r].n_name && 0 == strncmp("vhost", q[r].name, 5))
            {
                u->vhost = u->__ptr + n;
                r = snprintf(u->vhost, sizeof(u->__ptr) - n, "%.*s", q[r].n_value, q[r].value);
                if (r <= 0 || (size_t)r >= sizeof(u->__ptr) - n)
                {
                    uri_query_free(&q);
                    goto FAILED_PARSE_RTMP_URL;
                }
                n += r + 1;
                break;
            }
        }
        uri_query_free(&q);
    }
	uri_free(uri);
    
    p[0] = strstr(url, "://");
    p[0] = p[0] ? p[0] + 3 : url;
    p[0] = strchr(p[0], '/');
	if (!p[0]) return -1;
    
    for (i = 0; i+1 < sizeof(p)/sizeof(p[0]) && p[i]; i++)
    {
        // filter '/' (one ore more)
        while ('/' == *p[i]) p[i]++;
        pend[i] = strchr(p[i], '/');
        p[i+1] = pend[i];
    }

    if (i < 2 || i > 3)
        return -1; // invalid rtmp url

    j = 0;
    if (i > 2)
        v[j++] = &u->vhost;
    v[j++] = &u->app;
    v[j++] = &u->stream;
    for(j = 0; j < i; j++)
    {
        *v[j] = u->__ptr + n;
        r = url_decode(p[j], pend[j] ? (int)(pend[j] - p[j]) : -1, *v[j], sizeof(u->__ptr) - n);
        if (r <= 0 || (size_t)r >= sizeof(u->__ptr) - n)
            return -1;
        n += r + 1;
    }
    
    u->tcurl = u->__ptr + n;
    if(PORT != u->port)
	    r = snprintf(u->tcurl, sizeof(u->__ptr) - n, "rtmp://%s:%d/%s", u->vhost, u->port, u->app);
    else
        r = snprintf(u->tcurl, sizeof(u->__ptr) - n, "rtmp://%s/%s", u->vhost, u->app);
    if(r <= 0 || (size_t)r >= sizeof(u->__ptr) - n)
        return -1;
    n += r + 1;
    
    return 0;
    
FAILED_PARSE_RTMP_URL:
    uri_free(uri);
    return -1;
}

#if !defined(NDEBUG)
static inline void rtmp_url_parse_test(void)
{
    struct rtmp_url_t rtmp;
    assert(0 == rtmp_url_parse("rtmp://www.abc.com/app/stream", &rtmp));
    assert(0 == strcmp(rtmp.tcurl, "rtmp://www.abc.com/app") && 0 == strcmp(rtmp.app, "app") && 0 == strcmp(rtmp.stream, "stream"));
    assert(0 == strcmp(rtmp.scheme, "rtmp") && 0 == strcmp(rtmp.host, "www.abc.com") && 1935 == rtmp.port && 0 == strcmp(rtmp.vhost, "www.abc.com"));

    // stream name with parameter(s)
    assert(0 == rtmp_url_parse("rtmp://w.a.c/a/s?param1=value1&param2=value2", &rtmp));
    assert(0 == strcmp(rtmp.tcurl, "rtmp://w.a.c/a") && 0 == strcmp(rtmp.app, "a") && 0 == strcmp(rtmp.stream, "s?param1=value1&param2=value2"));
    assert(0 == strcmp(rtmp.scheme, "rtmp") && 0 == strcmp(rtmp.host, "w.a.c") && 1935 == rtmp.port && 0 == strcmp(rtmp.vhost, "w.a.c"));

    // tcurl with param
    assert(0 == rtmp_url_parse("rtmp://192.168.1.100:1936/www.abc.com/app?param1=value1&param2=value2/stream?param1=value1&param2=value2", &rtmp));
    assert(0 == strcmp(rtmp.tcurl, "rtmp://www.abc.com:1936/app?param1=value1&param2=value2") && 0 == strcmp(rtmp.app, "app?param1=value1&param2=value2") && 0 == strcmp(rtmp.stream, "stream?param1=value1&param2=value2"));
    assert(0 == strcmp(rtmp.scheme, "rtmp") && 0 == strcmp(rtmp.host, "192.168.1.100") && 1936 == rtmp.port && 0 == strcmp(rtmp.vhost, "www.abc.com"));

    // obs tcurl with '/'
    assert(0 == rtmp_url_parse("rtmp://www.abc.com:1936//app//stream", &rtmp));
    assert(0 == strcmp(rtmp.tcurl, "rtmp://www.abc.com:1936/app") && 0 == strcmp(rtmp.app, "app") && 0 == strcmp(rtmp.stream, "stream"));
    assert(0 == strcmp(rtmp.scheme, "rtmp") && 0 == strcmp(rtmp.host, "www.abc.com") && 1936 == rtmp.port && 0 == strcmp(rtmp.vhost, "www.abc.com"));

    // host + vhost
    assert(0 == rtmp_url_parse("rtmp://192.168.1.100:1936/www.abc.com/app//stream", &rtmp));
    assert(0 == strcmp(rtmp.tcurl, "rtmp://www.abc.com:1936/app") && 0 == strcmp(rtmp.app, "app") && 0 == strcmp(rtmp.stream, "stream"));
    assert(0 == strcmp(rtmp.scheme, "rtmp") && 0 == strcmp(rtmp.host, "192.168.1.100") && 1936 == rtmp.port && 0 == strcmp(rtmp.vhost, "www.abc.com"));

    // vhost parameter
    assert(0 == rtmp_url_parse("rtmp://192.168.1.100/app//stream?param1=value1&vhost=www.abc.com&param2=value2", &rtmp));
    assert(0 == strcmp(rtmp.tcurl, "rtmp://www.abc.com/app") && 0 == strcmp(rtmp.app, "app") && 0 == strcmp(rtmp.stream, "stream?param1=value1&vhost=www.abc.com&param2=value2"));
    assert(0 == strcmp(rtmp.scheme, "rtmp") && 0 == strcmp(rtmp.host, "192.168.1.100") && 1935 == rtmp.port && 0 == strcmp(rtmp.vhost, "www.abc.com"));

    // bad case: vhost + parameter
    assert(0 == rtmp_url_parse("rtmp://192.168.1.100/www.abc.com/app//stream?param1=value1&vhost=www.google.com&param2=value2", &rtmp));
    assert(0 == strcmp(rtmp.tcurl, "rtmp://www.abc.com/app") && 0 == strcmp(rtmp.app, "app") && 0 == strcmp(rtmp.stream, "stream?param1=value1&vhost=www.google.com&param2=value2"));
    assert(0 == strcmp(rtmp.scheme, "rtmp") && 0 == strcmp(rtmp.host, "192.168.1.100") && 1935 == rtmp.port && 0 == strcmp(rtmp.vhost, "www.abc.com"));

    // bad case: no stream
    assert(-1 == rtmp_url_parse("rtmp://192.168.1.100///stream?param1=value1&vhost=www.abc.com&param2=value2", &rtmp));
    
    // bad case: no path
    assert(-1 == rtmp_url_parse("rtmp://192.168.1.100/", &rtmp));
}
#endif

#endif /* !_rtmp_url_h_ */
