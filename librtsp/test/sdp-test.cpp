#include "sdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char s_sdp[64 * 1024];

static const char* sdp_read(const char* file)
{
	FILE* fp = fopen(file, "rb");
	fread(s_sdp, 1, sizeof(s_sdp), fp);
	fclose(fp);
	return s_sdp;
}

void sdp_test(const char* file)
{
	const char* txt = sdp_read(file);
	sdp_t* sdp = sdp_parse(txt, strlen(txt));
	sdp_destroy(sdp);
}

void sdp_test2()
{
	const char* txt = "v=0\n\
o=- 0 0 IN IP4 192.168.3.118\n\
s=-\n\
c=IN IP4 192.168.3.118\n\
m=video 10088 RTP/AVP 100\n\
a=rtpmap:100 H264/90000\n\
a=fmtp:100 CIF=1;4CIF=1;F=1;K=1\n\
f=v//5///a///\n\
a=sendrecv";
	sdp_t* sdp = sdp_parse(txt, strlen(txt));
	sdp_destroy(sdp);
}