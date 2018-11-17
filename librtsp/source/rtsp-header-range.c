// RFC 2326 Real Time Streaming Protocol (RTSP)
//
// 12.29 Range(p53)
// parameter: time. in UTC, specifying the time at which the operation is to be made effective
// Ranges are half-open intervals, including the lower point, but excluding the upper point.
// byte ranges MUST NOT be used
// Range = "Range" ":" 1#ranges-specifier [ ";" "time" "=" utc-time ]
// ranges-specifier = npt-range | utc-range | smpte-range
// smpte/smpte-30-drop/smpte-25: hh:mm:ss[:frames][.subframes] - [hh:mm:ss[:frames][.subframes]]  (p17)
// npt: hh:mm:ss[.ms]-[hh:mm:ss[.ms]] | -hh:mm:ss[.ms] | now-   (p17)
// clock: YYYYMMDDThhmmss[.fraction]Z - [YYYYMMDDThhmmss[.fraction]Z] (p18)
//
// e.g. 
// Range: clock=19960213T143205Z-;time=19970123T143720Z
// Range: smpte-25=10:07:00-10:07:33:05.01,smpte-25=11:07:00-11:07:33:05.01

#include "rtsp-header-range.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64) || defined(OS_WINDOWS)
#define strncasecmp	_strnicmp
#endif

struct time_smpte_t
{
	int second;		// [0,24*60*60)
	int frame;		// [0,99]
	int subframe;	// [0,99]
};

struct time_npt_t
{
	int64_t second;	// [0,---)
	int fraction;	// [0,999]
};

struct time_clock_t
{
	int second;		// [0,24*60*60)
	int fraction;	// [0,999]
	int day;		// [0,30]
	int month;		// [0,11]
	int year;		// [xxxx]
};

#define RANGE_SPECIAL ",;\r\n"

static uint64_t utc_mktime(const struct tm *t)
{
    int mon = t->tm_mon+1, year = t->tm_year+1900;

    /* 1..12 -> 11,12,1..10 */
    if (0 >= (int) (mon -= 2)) {
        mon += 12;  /* Puts Feb last since it has leap day */
        year -= 1;
    }
    
    return ((((uint64_t)
              (year/4 - year/100 + year/400 + 367*mon/12 + t->tm_mday) +
              year*365 - 719499
              )*24 + t->tm_hour /* now have hours */
             )*60 + t->tm_min /* now have minutes */
            )*60 + t->tm_sec; /* finally seconds */
}

static inline const char* string_token_int(const char* str, int *value)
{
	*value = 0;
	while ('0' <= *str && *str <= '9')
	{
		*value = (*value * 10) + (*str - '0');
		++str;
	}
	return str;
}

static inline const char* string_token_int64(const char* str, int64_t *value)
{
	*value = 0;
	while ('0' <= *str && *str <= '9')
	{
		*value = (*value * 10) + (*str - '0');
		++str;
	}
	return str;
}

// smpte-time = 1*2DIGIT ":" 1*2DIGIT ":" 1*2DIGIT [ ":" 1*2DIGIT ][ "." 1*2DIGIT ]
// hours:minutes:seconds:frames.subframes
static const char* rtsp_header_range_smpte_time(const char* str, int *hours, int *minutes, int *seconds, int *frames, int *subframes)
{
	const char* p;

	assert(str);
	p = string_token_int(str, hours);
	if(*p != ':')
		return NULL;

	p = string_token_int(p+1, minutes);
	if(*p != ':')
		return NULL;

	p = string_token_int(p+1, seconds);

	*frames = 0;
	*subframes = 0;
	if(*p == ':')
	{
		p = string_token_int(p+1, frames);
	}

	if(*p == '.')
	{
		p = string_token_int(p+1, subframes);
	}

	return p;
}

// 3.5 SMPTE Relative Timestamps (p16)
// smpte-range = smpte-type "=" smpte-time "-" [ smpte-time ]
// smpte-type = "smpte" | "smpte-30-drop" | "smpte-25"
// Examples:
//	smpte=10:12:33:20-
//	smpte=10:07:33-
//	smpte=10:07:00-10:07:33:05.01
//	smpte-25=10:07:00-10:07:33:05.01
static int rtsp_header_range_smpte(const char* fields, struct rtsp_header_range_t* range)
{
	const char *p;
	int hours, minutes, seconds, frames, subframes;

	assert(fields);
	p = rtsp_header_range_smpte_time(fields, &hours, &minutes, &seconds, &frames, &subframes);
	if(!p || '-' != *p)
		return -1;

	range->from_value = RTSP_RANGE_TIME_NORMAL;
	range->from = (hours%24)*3600 + (minutes%60)*60 + seconds;
	range->from = range->from * 1000 + frames % 1000;

	assert('-' == *p);
	if('\0' == p[1] || strchr(RANGE_SPECIAL, p[1]))
	{
		range->to_value = RTSP_RANGE_TIME_NOVALUE;
		range->to = 0;
	}
	else
	{
		p = rtsp_header_range_smpte_time(p+1, &hours, &minutes, &seconds, &frames, &subframes);
		if(!p ) return -1;
		assert('\0' == p[0] || strchr(RANGE_SPECIAL, p[0]));

		range->to_value = RTSP_RANGE_TIME_NORMAL;
		range->to = (hours%24)*3600 + (minutes%60)*60 + seconds;
		range->to = range->to * 1000 + frames % 1000;
	}

	return 0;
}

// npt-time = "now" | npt-sec | npt-hhmmss
// npt-sec = 1*DIGIT [ "." *DIGIT ]
// npt-hhmmss = npt-hh ":" npt-mm ":" npt-ss [ "." *DIGIT ]
// npt-hh = 1*DIGIT ; any positive number
// npt-mm = 1*2DIGIT ; 0-59
// npt-ss = 1*2DIGIT ; 0-59
static const char* rtsp_header_range_npt_time(const char* str, uint64_t *seconds, int *fraction)
{
	const char* p;
	int v1, v2;

	assert(str);
	str += strspn(str, " \t");
	p = strpbrk(str, "-\r\n");
	if(!str || (p-str==3 && 0==strncasecmp(str, "now", 3)))
	{
		*seconds = 0; // now
		*fraction = -1;
	}
	else
	{
		p = string_token_int64(str, (int64_t*)seconds);
		if(*p == ':')
		{
			// npt-hhmmss
			p = string_token_int(p+1, &v1);
			if(*p != ':')
				return NULL;

			p = string_token_int(p+1, &v2);

			assert(0 <= v1 && v1 < 60);
			assert(0 <= v2 && v2 < 60);
			*seconds = *seconds * 3600 + (v1%60)*60 + v2%60;
		}
		else
		{
			// npt-sec
			//*seconds = hours;
		}

		if(*p == '.')
			p = string_token_int(p+1, fraction);
		else
			*fraction = 0;
	}

	return p;
}

// 3.6 Normal Play Time (p17)
// npt-range = ( npt-time "-" [ npt-time ] ) | ( "-" npt-time )
// Examples:
//	npt=123.45-125
//	npt=12:05:35.3-
//	npt=now-
static int rtsp_header_range_npt(const char* fields, struct rtsp_header_range_t* range)
{
	const char* p;
	uint64_t seconds;
	int fraction;

	p = fields;
	if('-' != *p)
	{
		p = rtsp_header_range_npt_time(p, &seconds, &fraction);
		if(!p || '-' != *p)
			return -1;

		if(0 == seconds && -1 == fraction)
		{
			range->from_value = RTSP_RANGE_TIME_NOW;
			range->from = 0L;
		}
		else
		{
			range->from_value = RTSP_RANGE_TIME_NORMAL;
			range->from = seconds;
			range->from = range->from * 1000 + fraction % 1000;
		}
	}
	else
	{
		range->from_value = RTSP_RANGE_TIME_NOVALUE;
		range->from = 0;
	}

	assert('-' == *p);
	if('\0' == p[1] || strchr(RANGE_SPECIAL, p[1]))
	{
		assert('-' != *fields);
		range->to_value = RTSP_RANGE_TIME_NOVALUE;
		range->to = 0;
	}
	else
	{
		p = rtsp_header_range_npt_time(p+1, &seconds, &fraction);
		if( !p ) return -1;
		assert('\0' == p[0] || strchr(RANGE_SPECIAL, p[0]));

		range->to_value = RTSP_RANGE_TIME_NORMAL;
		range->to = seconds;
		range->to = range->to * 1000 + fraction % 1000;
	}

	return 0;
}

// utc-time = utc-date "T" utc-time "Z"
// utc-date = 8DIGIT ; < YYYYMMDD >
// utc-time = 6DIGIT [ "." fraction ] ; < HHMMSS.fraction >
static const char* rtsp_header_range_clock_time(const char* str, uint64_t *second, int *fraction)
{
	struct tm t;
	const char* p;
	int year, month, day;
	int hour, minute;

	assert(str);
	if(!str || 5 != sscanf(str, "%4d%2d%2dT%2d%2d", &year, &month, &day, &hour, &minute))
		return NULL;

	*second = 0;
	*fraction = 0;
	p = string_token_int64(str + 13, (int64_t*)second);
    assert(p);
	if(*p == '.')
	{
		p = string_token_int(p+1, fraction);
	}

	memset(&t, 0, sizeof(t));
	t.tm_year = year - 1900;
	t.tm_mon = month - 1;
	t.tm_mday = day;
	t.tm_hour = hour;
	t.tm_min = minute;
//	*second += mktime(&t);
    *second += utc_mktime(&t);

//	assert('Z' == *p);
//	assert('\0' == p[1] || strchr(RANGE_SPECIAL"-", p[1]));
	return 'Z'==*p ? p+1 : p;
}

// 3.7 Absolute Time (p18)
// utc-range = "clock" "=" utc-time "-" [ utc-time ]
// Range: clock=19961108T143720.25Z-
// Range: clock=19961110T1925-19961110T2015 (p72)
static int rtsp_header_range_clock(const char* fields, struct rtsp_header_range_t* range)
{
	const char* p;
	uint64_t second;
	int fraction;

	p = rtsp_header_range_clock_time(fields, &second, &fraction);
	if(!p || '-' != *p)
		return -1;

	range->from_value = RTSP_RANGE_TIME_NORMAL;
	range->from = second * 1000;
	range->from += fraction % 1000;

	assert('-' == *p);
	if('\0'==p[1] || strchr(RANGE_SPECIAL, p[1]))
	{
		range->to_value = RTSP_RANGE_TIME_NOVALUE;
		range->to = 0;
	}
	else
	{
		p = rtsp_header_range_clock_time(p+1, &second, &fraction);
		if( !p ) return -1;

		range->to_value = RTSP_RANGE_TIME_NORMAL;
		range->to = second * 1000;
		range->to += (unsigned int)fraction % 1000;
	}

	return 0;
}

int rtsp_header_range(const char* field, struct rtsp_header_range_t* range)
{
	int r = 0;

	range->time = 0L;
	while(field && 0 == r)
	{
		if(0 == strncasecmp("clock=", field, 6))
		{
			range->type = RTSP_RANGE_CLOCK;
			r = rtsp_header_range_clock(field+6, range);
		}
		else if(0 == strncasecmp("npt=", field, 4))
		{
			range->type = RTSP_RANGE_NPT;
			r = rtsp_header_range_npt(field+4, range);
		}
		else if(0 == strncasecmp("smpte=", field, 6))
		{
			range->type = RTSP_RANGE_SMPTE;
			r = rtsp_header_range_smpte(field+6, range);
			if(RTSP_RANGE_TIME_NORMAL == range->from_value)
				range->from = (range->from/1000 * 1000) + (1000/30 * (range->from%1000)); // frame to ms
			if(RTSP_RANGE_TIME_NORMAL == range->to_value)
				range->to = (range->to/1000 * 1000) + (1000/30 * (range->to%1000)); // frame to ms
		}
		else if(0 == strncasecmp("smpte-30-drop=", field, 15))
		{
			range->type = RTSP_RANGE_SMPTE_30;
			r = rtsp_header_range_smpte(field+15, range);
			if(RTSP_RANGE_TIME_NORMAL == range->from_value)
				range->from = (range->from/1000 * 1000) + (1000/30 * (range->from%1000)); // frame to ms
			if(RTSP_RANGE_TIME_NORMAL == range->to_value)
				range->to = (range->to/1000 * 1000) + (1000/30 * (range->to%1000)); // frame to ms
		}
		else if(0 == strncasecmp("smpte-25=", field, 9))
		{
			range->type = RTSP_RANGE_SMPTE_25;
			r = rtsp_header_range_smpte(field+9, range);
			if(RTSP_RANGE_TIME_NORMAL == range->from_value)
				range->from = (range->from/1000 * 1000) + (1000/25 * (range->from%1000)); // frame to ms
			if(RTSP_RANGE_TIME_NORMAL == range->to_value)
				range->to = (range->to/1000 * 1000) + (1000/25 * (range->to%1000)); // frame to ms
		}
		else if(0 == strncasecmp("time=", field, 5))
		{
			if (rtsp_header_range_clock_time(field + 5, &range->time, &r))
				range->time = range->time * 1000 + r % 1000;
		}
		
		field = strchr(field, ';');
		if(field)
			++field;
	}

	return r;
}

#if defined(DEBUG) || defined(_DEBUG)
void rtsp_header_range_test(void)
{
	struct tm t;
	struct rtsp_header_range_t range;

	// smpte
	assert(0==rtsp_header_range("smpte=10:12:33:20-", &range));
	assert(range.type == RTSP_RANGE_SMPTE && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NOVALUE);
	assert(range.from == (10*3600+12*60+33)*1000 + 20*33);

	assert(0==rtsp_header_range("smpte=10:07:33-", &range));
	assert(range.type == RTSP_RANGE_SMPTE && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NOVALUE);
	assert(range.from == (10*3600+7*60+33)*1000);

	assert(0==rtsp_header_range("smpte=10:07:00-10:07:33:05.01", &range));
	assert(range.type == RTSP_RANGE_SMPTE && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NORMAL);
	assert(range.from == (10*3600+7*60)*1000);
	assert(range.to == (10*3600+7*60+33)*1000 + 5*33);

	assert(0==rtsp_header_range("smpte-25=10:07:00-10:07:33:05.01", &range));
	assert(range.type == RTSP_RANGE_SMPTE_25 && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NORMAL);
	assert(range.from == (10*3600+7*60)*1000);
	assert(range.to == (10*3600+7*60+33)*1000 + 5*40);

	// npt
	assert(0==rtsp_header_range("npt=now-", &range));
	assert(range.type == RTSP_RANGE_NPT && range.from_value == RTSP_RANGE_TIME_NOW && range.to_value == RTSP_RANGE_TIME_NOVALUE);

	assert(0==rtsp_header_range("npt=12:05:35.3-", &range));
	assert(range.type == RTSP_RANGE_NPT && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NOVALUE);
	assert(range.from == (12*3600+5*60+35)*1000 + 3);

	assert(0==rtsp_header_range("npt=123.45-125", &range));
	assert(range.type == RTSP_RANGE_NPT && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NORMAL);
	assert(range.from == 123*1000+45);
	assert(range.to == 125*1000);

	// clock
	assert(-1==rtsp_header_range("clock=-19961108T143720.25Z", &range));

	assert(0==rtsp_header_range("clock=19961108T143720.25Z-", &range));
	assert(range.type == RTSP_RANGE_CLOCK && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NOVALUE);
	t.tm_year = 1996-1900; t.tm_mon = 11-1; t.tm_mday = 8; t.tm_hour = 14; t.tm_min = 37; t.tm_sec = 20;
	assert(range.from == utc_mktime(&t)*1000 + 25);

	assert(0==rtsp_header_range("clock=19961110T1925-19961110T2015", &range)); // rfc2326 (p72)
	assert(range.type == RTSP_RANGE_CLOCK && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NORMAL);
	t.tm_year = 1996-1900; t.tm_mon = 11-1; t.tm_mday = 10; t.tm_hour = 19; t.tm_min = 25; t.tm_sec = 00;
	assert(range.from == utc_mktime(&t)*1000);
	t.tm_year = 1996-1900; t.tm_mon = 11-1; t.tm_mday = 10; t.tm_hour = 20; t.tm_min = 15; t.tm_sec = 00;
	assert(range.to == utc_mktime(&t)*1000);

	// time
	assert(0 == rtsp_header_range("smpte=0:10:20-;time=19970123T153600Z", &range)); // rfc2326 (p35)
	assert(range.type == RTSP_RANGE_SMPTE && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NOVALUE);
	assert(range.from == (10 * 60 + 20) * 1000);
	t.tm_year = 1997 - 1900; t.tm_mon = 1 - 1; t.tm_mday = 23; t.tm_hour = 15; t.tm_min = 36; t.tm_sec = 00;
	assert(range.time == utc_mktime(&t) * 1000);
}
#endif
