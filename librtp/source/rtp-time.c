#include <stdint.h>
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

uint64_t rtpclock()
{
#if defined(OS_WINDOWS)
    LARGE_INTEGER freq;
    LARGE_INTEGER count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)count.QuadPart * 1000 / freq.QuadPart;
#elif defined(OS_MAC)
    uint64_t tick;
    mach_timebase_info_data_t timebase;
    tick = mach_absolute_time();
    mach_timebase_info(&timebase);
    return tick * timebase.numer / timebase.denom / 1000000;
#else
#if defined(CLOCK_MONOTONIC)
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (uint64_t)tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
#else
    // POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
#endif
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
	ntp |= (uint32_t)(((clock % 1000) * 1000) * 0x4000000 / 15625);

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
