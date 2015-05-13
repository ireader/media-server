#include "time64.h"

time64_t clock2ntp(time64_t clock)
{
	time64_t ntp;

	// high 32 bits in seconds
	ntp = ((clock/1000)+0x83AA7E80) << 32; // 1/1/1970 -> 1/1/1900

	// low 32 bits in picosecond
	// ms * 2^32 / 10^3
	// 10^6 = 2^6 * 15625
	// => ms * 1000 * 2^26 / 15625
	ntp |= ((clock % 1000) * 1000) * 0x4000000 / 15625;

	return ntp;
}

time64_t ntp2clock(time64_t ntp)
{
	time64_t clock;

	// high 32 bits in seconds
	clock = ((time64_t)((unsigned int)(ntp >> 32) - 0x83AA7E80)) * 1000; // 1/1/1900 -> 1/1/1970

	// low 32 bits in picosecond
	clock += (unsigned int)(((unsigned int)(ntp & 0xFFFFFFFF)) * 15.625 / 0x4000000);

	return clock;
}
