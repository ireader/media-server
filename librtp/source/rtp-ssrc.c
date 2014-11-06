#include <math.h>
#include <time.h>
#include <stdlib.h>

int rtp_ssrc(void)
{
	int ssrc;
	srand((unsigned int)time(NULL));
	ssrc = rand();
	return ssrc;
}
