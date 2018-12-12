#pragma once

#include "uri-parse.h"
#include <vector>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct RTMPUrl
{
	int port;
	std::string host;
	std::string app;
	std::string stream;
	std::string tcurl;

	RTMPUrl(const std::string& url) : port(0), tcurl(url)
	{
		Parse(url.c_str());
	}

	bool Valid() const
	{
		return host.size() && app.size() && stream.size() && port;
	}

private:
	int Parse(const char* rtmp)
	{
		uri_t* uri = uri_parse(rtmp, strlen(rtmp));
		if (NULL == uri)
			return -1;

		host = uri->host;
		port = uri->port ? uri->port : 1935; // rtmp default port

		const char* p1 = strchr(rtmp + ((uri->scheme&&*uri->scheme) ? (strlen(uri->scheme) + 3) : 1), '/');
		if (!p1) return -1;
		const char* p2 = strchr(p1 + 1, '/');
		if (!p2) return -1;

		app.assign(p1 + 1, p2 - p1 - 1);
		stream.assign(p2 + 1);
		tcurl.assign(rtmp, p2);
		uri_free(uri);
		return 0;
	}
};
