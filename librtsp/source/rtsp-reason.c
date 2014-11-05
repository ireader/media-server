#include "http-reason.h"

const char* rtsp_reason_phrase(int code)
{
	static const char *reason45x[] = 
	{
		"Parameter Not Understood", // 451
		"Conference Not Found", // 452
		"Not Enough Bandwidth", // 453
		"Session Not Found", // 454
		"Method Not Valid in This State", // 455
		"Header Field Not Valid for Resource", // 456
		"Invalid Range", // 457
		"Parameter Is Read-Only", // 458
		"Aggregate Operation Not Allowed", // 459
		"Only Aggregate Operation Allowed", // 460
		"Unsupported Transport", // 461
		"Destination Unreachable", // 462
	};

	if(451 <= code && code < 451+sizeof(reason45x)/sizeof(reason45x[0]))
		return reason45x[code-451];

	switch(code)
	{
	case 505:
		return "RTSP Version Not Supported";
	case 551:
		return "Option not supported";
	default:
		return http_reason_phrase(code);
	}
}
