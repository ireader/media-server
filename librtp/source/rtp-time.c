#include <stdint.h>
#include <assert.h>
#include <time.h>

#if defined(OS_WINDOWS)
#include <Windows.h>
#else
#include <sys/time.h>

#if defined(OS_MAC)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <mach/mach_time.h>
#endif

#endif

/// same as system_time except ms -> us
/// @return microseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
uint64_t rtpclock()
{
#if defined(OS_WINDOWS)
	uint64_t t;
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	t = (uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
	return t / 10 - 11644473600000000; /* Jan 1, 1601 */
#elif defined(OS_MAC)
    uint64_t tick;
    mach_timebase_info_data_t timebase;
    tick = mach_absolute_time();
    mach_timebase_info(&timebase);
    return tick * timebase.numer / timebase.denom / 1000;
#else
    // POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}

/// us(microsecond) -> ntp
uint64_t clock2ntp(uint64_t clock)
{
	uint64_t ntp;

	// high 32 bits in seconds
	ntp = ((clock/1000000)+0x83AA7E80) << 32; // 1/1/1970 -> 1/1/1900

	// low 32 bits in picosecond
	// us * 2^32 / 10^6
	// 10^6 = 2^6 * 15625
	// => us * 2^26 / 15625
	ntp |= (uint32_t)(((clock % 1000000) << 26) / 15625);

	return ntp;
}

/// ntp -> us(microsecond)
uint64_t ntp2clock(uint64_t ntp)
{
	uint64_t clock;

	// high 32 bits in seconds
	clock = ((uint64_t)((unsigned int)(ntp >> 32) - 0x83AA7E80)) * 1000000; // 1/1/1900 -> 1/1/1970

	// low 32 bits in picosecond
	clock += ((ntp & 0xFFFFFFFF) * 15625) >> 26;

	return clock;
}

#if defined(_DEBUG) || defined(DEBUG)
void rtp_time_test(void)
{
	const uint64_t ntp = 0xe2e1d897e9c38b05ULL;
	uint64_t clock;
	struct tm* tm;
	time_t t;

	clock = ntp2clock(ntp);
	t = (time_t)(clock / 1000000);
	tm = gmtime(&t);
	assert(tm->tm_year + 1900 == 2020 && tm->tm_mon == 7 && tm->tm_mday == 15 && tm->tm_hour == 3 && tm->tm_min == 44 && tm->tm_sec == 23);
	assert(clock2ntp(clock) == 0xe2e1d897e9c38b04ULL);
}
#endif
