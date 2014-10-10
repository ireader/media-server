// RFC-4566 SDP
// 6. SDP Attributes (p25)
// a=rtpmap:<payload type> <encoding name>/<clock rate>[/<encoding parameters>]
//
// a=rtpmap:98 L16/16000/2
//
// m=<media> <port>/<number of ports> <transport> <fmt list>
// m=audio 49230 RTP/AVP 96 97 98
// a=rtpmap:96 L8/8000
// a=rtpmap:97 L16/8000
// a=rtpmap:98 L16/11025/2

#include <stdlib.h>
#include <string.h>
#include <assert.h>

int sdp_a_rtpmap(const char* rtpmap, int *payload, char *encoding, int *rate, char *parameters)
{
	const char *p1;
	const char *p = rtpmap;

	// payload type
	p1 = strchr(p, ' ');
	if(' ' != *p1)
		return -1;

	if(payload)
	{
		*payload = atoi(p);
	}
	p = p1 + 1;

	// encoding name
	assert(' ' == *p1);
	p1 = strchr(p, '/');
	if('/' != *p1)
		return -1;

	if(encoding)
	{
		strncpy(encoding, p, p1-p);
		encoding[p1-p] = '\0';
	}
	p = p1 + 1;

	// clock rate	
	assert('/' == *p1);
	if(rate)
	{
		*rate = atoi(p);
	}

	// encoding parameters
	p1 = strchr(p, '/');
	if(p1 && '/' == *p1 && parameters)
	{
		strcpy(parameters, p1+1);
	}

	return 0;
}
