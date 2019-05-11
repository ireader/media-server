#include "rtsp-server-internal.h"

int rtsp_server_get_parameter(struct rtsp_server_t *rtsp, const char* uri)
{
	int bytes;
	const void *content;
	content = http_get_content(rtsp->parser);
	bytes = http_get_content_length(rtsp->parser);
	return rtsp->handler.ongetparameter(rtsp->param, rtsp, uri, rtsp->session.session[0] ? rtsp->session.session : NULL, content, bytes);
}

int rtsp_server_set_parameter(struct rtsp_server_t *rtsp, const char* uri)
{
	int bytes;
	const void *content;
	content = http_get_content(rtsp->parser);
	bytes = http_get_content_length(rtsp->parser);
	return rtsp->handler.onsetparameter(rtsp->param, rtsp, uri, rtsp->session.session[0] ? rtsp->session.session : NULL, content, bytes);
}

int rtsp_server_reply_get_parameter(struct rtsp_server_t *rtsp, int code, const void* content, int bytes)
{
	static const char* headers[] = { "Content-Type", "Content-Encoding",  "Content-Language" };

	int i, len;
	char buffer[1024];
	const char* value;

	// copy headers
	buffer[0] = 0;
	for (len = i = 0; i < sizeof(headers) / sizeof(headers[0]); i++)
	{
		value = http_get_header_by_name(rtsp->parser, headers[i]);
		if (value)
			len += snprintf(buffer + len, sizeof(buffer) - len, "%s: %s\r\n", headers[i], value);
	}

	if (len < 0 || len >= sizeof(buffer))
	{
		assert(0); // headers too long
		return -1;
	}

	return rtsp_server_reply2(rtsp, code, buffer, content, bytes);
}

int rtsp_server_reply_set_parameter(struct rtsp_server_t *rtsp, int code)
{
	return rtsp_server_reply(rtsp, code);
}
