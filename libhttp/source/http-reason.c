#include "http-reason.h"

const char* http_reason_phrase(int code)
{
	static const char *reason1xx[] = 
	{
		"Continue", // 100
		"Switching Protocols" // 101
	};

	static const char *reason2xx[] = 
	{
		"OK", // 200
		"Created", // 201
		"Accepted", // 202
		"Non-Authoritative Information", // 203
		"No Content", // 204
		"Reset Content", // 205
		"Partial Content" // 206
	};

	static const char *reason3xx[] = 
	{
		"Multiple Choices", // 300
		"Move Permanently", // 301
		"Found", // 302
		"See Other", // 303
		"Not Modified", // 304
		"Use Proxy", // 305
		"Unused", // 306
		"Temporary Redirect" // 307
	};

	static const char *reason4xx[] = 
	{
		"Bad Request", // 400
		"Unauthorized", // 401
		"Payment Required", // 402
		"Forbidden", // 403
		"Not Found", // 404
		"Method Not Allowed", // 405
		"Not Acceptable", // 406
		"Proxy Authentication Required", // 407
		"Request Timeout", // 408
		"Conflict", // 409
		"Gone", // 410
		"Length Required", //411
		"Precondition Failed", // 412
		"Request Entity Too Large", // 413
		"Request-URI Too Long", // 414
		"Unsupported Media Type", // 415
		"Request Range Not Satisfiable", // 416
		"Expectation Failed" // 417
	};

	static const char *reason5xx[] = 
	{
		"Internal Server Error", // 500
		"Not Implemented", // 501
		"Bad Gateway", // 502
		"Service Unavailable", // 503
		"Gateway Timeout", // 504
		"HTTP Version Not Supported" // 505
	};

	if(code >= 100 && code < 100+sizeof(reason1xx)/sizeof(reason1xx[0]))
		return reason1xx[code-100];
	else if(code >= 200 && code < 200+sizeof(reason2xx)/sizeof(reason2xx[0]))
		return reason2xx[code-200];
	else if(code >= 300 && code < 300+sizeof(reason3xx)/sizeof(reason3xx[0]))
		return reason3xx[code-300];
	else if(code >= 400 && code < 400+sizeof(reason4xx)/sizeof(reason4xx[0]))
		return reason4xx[code-400];
	else if(code >= 500 && code < 500+sizeof(reason5xx)/sizeof(reason5xx[0]))
		return reason5xx[code-500];
	else
		return "unknown";
}
