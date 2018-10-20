#pragma once

#include "url.h"
#include "urlcodec.h"
#include "cppstringext.h"
#include <assert.h>
#include <vector>
#include <string>

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
		void* url = url_parse(rtmp);
		if (NULL == url)
			return -1;

		host = url_gethost(url);
		port = url_getport(url);
		port = port ? port : 1935; // rtmp default port

		const char* path = url_getpath(url);
		if (NULL == path)
		{
			url_free(url);
			return -1;
		}

		char p[128] = { 0 };
		url_decode(path, -1, p, sizeof(p));

		std::vector<std::string> paths;
		Split(p, '/', paths);
		if (paths.size() < 3 || !paths[0].empty())
		{
			url_free(url);
			return -1;
		}

		app = paths[1];
		stream = paths[2];

		url_free(url);
	}
};
