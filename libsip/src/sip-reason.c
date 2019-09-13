#include "http-reason.h"

const char* sip_reason_phrase(int code)
{
	static const char *reason18x[] =
	{
		"Ringing", // 180
		"Call Is Being Forwarded", // 181
		"Queued", // 182
		"Session Progress", // 183
	};

	static const char *reason48x[] =
	{
		// 469 Bad Info Package // rfc6086
		"Temporarily Unavailable", // 480
		"Call/Transaction Does Not Exist", // 481
		"Loop Detected", // 482
		"Too Many Hops", // 483
		"Address Incomplete", // 484
		"Ambiguous", // 485
		"Busy Here", // 486
		"Request Terminated", // 487
		"Not Acceptable Here", // 488
		"", // 489
		"", // 490
		"Request Pending", // 491
		"", // 492
		"Undecipherable", // 493
	};

	static const char *reason6xx[] =
	{
		"Busy Everywhere", // 600
		"", // 601
		"", // 602
		"Decline", // 603
		"Does Not Exist Anywhere", // 604
		"", // 605
		"Not Acceptable", // 606
	};

	if (180 <= code && code < 180 + sizeof(reason18x) / sizeof(reason18x[0]))
		return reason18x[code - 180];
	else if (480 <= code && code < 480 + sizeof(reason48x) / sizeof(reason48x[0]))
		return reason48x[code - 480];
	else if (600 <= code && code < 600 + sizeof(reason6xx) / sizeof(reason6xx[0]))
		return reason6xx[code - 600];

	switch (code)
	{
	case 100:
		return "Trying";
	case 380:
		return "Alternative Service";
	default:
		return http_reason_phrase(code);
	}
}
