#include "http-reason.h"

const char* rtsp_reason_phrase(int code)
{
	switch(code)
	{
	case 451:
		return "Parameter Not Understood";
	case 452:
		return "Conference Not Found";
	case 453:
		return "Not Enough Bandwidth";
	case 454:
		return "Session Not Found";
	case 455:
		return "Method Not Valid in This State";
	case 456:
		return "Header Field Not Valid for Resource";
	case 457:
		return "Invalid Range";
	case 458:
		return "Parameter Is Read-Only";
	case 459:
		return "Aggregate Operation Not Allowed";
	case 460:
		return "Only Aggregate Operation Allowed";
	case 461:
		return "Unsupported Transport";
	case 462:
		return "Destination Unreachable";
	case 505:
		return "RTSP Version Not Supported";
	case 551:
		return "Option not supported";
	default:
		return http_reason_phrase(code);
	}
}
