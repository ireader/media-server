#include <math.h>
#include <time.h>
#include <stdlib.h>

int rtp_ssrc(void)
{
	static unsigned int seed = 0;
	if (0 == seed)
	{
		seed = (unsigned int)time(NULL);
		srand(seed);
	}
	return rand();
}
