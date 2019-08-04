#include "sdp.h"
#include <stdio.h>

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
	sdp_t* sdp = sdp_parse(txt);
	sdp_destroy(sdp);
}
