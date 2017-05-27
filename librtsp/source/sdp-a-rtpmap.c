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

#include "sdp-a-rtpmap.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int sdp_a_rtpmap(const char* rtpmap, int *payload, char *encoding, int *rate, char *parameters)
{
	const char *p1;
	const char *p = rtpmap;

	// payload type
	p1 = strchr(p, ' ');
	if(!p1)
		return -1;

	if(payload)
	{
		*payload = atoi(p);
	}
	p = p1 + 1;

	// encoding name
	assert(' ' == *p1);
	p1 = strchr(p, '/');
	if(!p1)
		return -1;

	if(encoding)
	{
        memcpy(encoding, p, p1-p);
		encoding[p1-p] = '\0';
	}

	// clock rate	
	assert('/' == *p1);
	if(rate)
	{
		*rate = atoi(p1+1);
	}

	// encoding parameters
    if(parameters)
    {
        p1 = strchr(p1+1, '/');
        if(p1)
        {
            strcpy(parameters, p1+1);
        }
        else
        {
            parameters[0] = '\0';
        }
    }

	return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
void sdp_a_rtpmap_test(void)
{
	int payload = 0;
	int rate = 0;
	char encoding[32];
	char parameters[64];

	assert(0 == sdp_a_rtpmap("96 L8/8000", &payload, encoding, &rate, parameters));
	assert(96 == payload && 8000 == rate && 0 == strcmp(encoding, "L8") && '\0' == parameters[0]);

	assert(0 == sdp_a_rtpmap("97 L16/8000", &payload, encoding, &rate, parameters));
	assert(97 == payload && 8000 == rate && 0 == strcmp(encoding, "L16") && '\0' == parameters[0]);

	assert(0 == sdp_a_rtpmap("98 L16/11025/2", &payload, encoding, &rate, parameters));
	assert(98 == payload && 11025 == rate && 0 == strcmp(encoding, "L16") && 0==strcmp("2", parameters));

	assert(0 == sdp_a_rtpmap("102 G726-16/8000", &payload, encoding, &rate, parameters));
	assert(102 == payload && 8000 == rate && 0 == strcmp(encoding, "G726-16") && 0 == strcmp("", parameters));
}
#endif
