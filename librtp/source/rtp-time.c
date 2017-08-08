#include <stdint.h>
#include <time.h>

#if defined(OS_WINDOWS)
#include <Windows.h>
#else
#include <sys/time.h>
#endif

uint64_t rtpclock()
{
	uint64_t v;
#if defined(OS_WINDOWS)
	FILETIME ft;
	GetSystemTimeAsFileTime((FILETIME*)&ft);
	v = (((__int64)ft.dwHighDateTime << 32) | (__int64)ft.dwLowDateTime) / 10000; // to ms
	v -= 0xA9730B66800; /* 11644473600000LL */ // January 1, 1601 (UTC) -> January 1, 1970 (UTC).
#else
	struct timeval tv;	
	gettimeofday(&tv, NULL);
	v = tv.tv_sec;
	v *= 1000;
	v += tv.tv_usec / 1000;
#endif
	return v;
}

uint64_t clock2ntp(uint64_t clock)
{
	uint64_t ntp;

	// high 32 bits in seconds
	ntp = ((clock/1000)+0x83AA7E80) << 32; // 1/1/1970 -> 1/1/1900

	// low 32 bits in picosecond
	// ms * 2^32 / 10^3
	// 10^6 = 2^6 * 15625
	// => ms * 1000 * 2^26 / 15625
	ntp |= ((clock % 1000) * 1000) * 0x4000000 / 15625;

	return ntp;
}

uint64_t ntp2clock(uint64_t ntp)
{
	uint64_t clock;

	// high 32 bits in seconds
	clock = ((uint64_t)((unsigned int)(ntp >> 32) - 0x83AA7E80)) * 1000; // 1/1/1900 -> 1/1/1970

	// low 32 bits in picosecond
	clock += (unsigned int)(((unsigned int)(ntp & 0xFFFFFFFF)) * 15.625 / 0x4000000);

	return clock;
}
