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
#include "rtsp-util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define RANGE_SPECIAL ",;\r\n"

// smpte-time = 1*2DIGIT ":" 1*2DIGIT ":" 1*2DIGIT [ ":" 1*2DIGIT ][ "." 1*2DIGIT ]
// hours:minutes:seconds:frames.subframes
static const char* rtsp_header_range_smpte_time(const char* str, int *hours, int *minutes, int *seconds, int *frames, int *subframes)
{
	const char* p;

	assert(str);
	p = string_token_number(str, hours);
	if(*p != ':')
		return NULL;

	p = string_token_number(p+1, minutes);
	if(*p != ':')
		return NULL;

	p = string_token_number(p+1, seconds);

	*frames = 0;
	*subframes = 0;
	if(*p == ':')
	{
		p = string_token_number(p+1, frames);
	}

	if(*p == '.')
	{
		p = string_token_number(p+1, subframes);
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
	range->smpte.from.second = (hours%24)*3600 + (minutes%60)*60 + seconds;
	range->smpte.from.frame = frames;
	range->smpte.from.subframe = subframes;

	assert('-' == *p);
	if('\0' == p[1] || strchr(RANGE_SPECIAL, p[1]))
	{
		range->to_value = RTSP_RANGE_TIME_NOVALUE;
		memset(&range->smpte.to, 0, sizeof(range->smpte.to));
	}
	else
	{
		if(!(p = rtsp_header_range_smpte_time(p+1, &hours, &minutes, &seconds, &frames, &subframes)) )
			return -1;
		assert('\0' == p[0] || strchr(RANGE_SPECIAL, p[0]));

		range->to_value = RTSP_RANGE_TIME_NORMAL;
		range->smpte.from.second = (hours%24)*3600 + (minutes%60)*60 + seconds;
		range->smpte.to.frame = frames;
		range->smpte.to.subframe = subframes;
	}

	return 0;
}

// npt-time = "now" | npt-sec | npt-hhmmss
// npt-sec = 1*DIGIT [ "." *DIGIT ]
// npt-hhmmss = npt-hh ":" npt-mm ":" npt-ss [ "." *DIGIT ]
// npt-hh = 1*DIGIT ; any positive number
// npt-mm = 1*2DIGIT ; 0-59
// npt-ss = 1*2DIGIT ; 0-59
static const char* rtsp_header_range_npt_time(const char* str, int64_t *seconds, int *fraction)
{
	const char* p;
	int v1, v2;

	assert(str);
	p = string_token_word(str, "-\r\n");
	if(p-str==3 && 0==strnicmp(str, "now", 3))
	{
		*seconds = -1; // now
		*fraction = 0;
		p += 3;
	}
	else
	{
		p = string_token_int64(str, seconds);
		if(*p == ':')
		{
			// npt-hhmmss
			p = string_token_number(p+1, &v1);
			if(*p != ':')
				return NULL;

			p = string_token_number(p+1, &v2);

			assert(0 <= v1 && v1 < 60);
			assert(0 <= v2 && v2 < 60);
			*seconds = *seconds*3600 + (v1%60)*60 + v2%60;
		}
		else
		{
			// npt-sec
			//*seconds = hours;
		}

		if(*p == '.')
			p = string_token_number(p+1, fraction);
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
	int64_t seconds;
	int fraction;

	p = fields;
	if('-' != *p)
	{
		p = rtsp_header_range_npt_time(p, &seconds, &fraction);
		if(!p || '-' != *p)
			return -1;

		if(-1 == seconds && 0 == fraction)
		{
			range->from_value = RTSP_RANGE_TIME_NOW;
			memset(&range->npt.from, 0, sizeof(range->npt.from));
		}
		else
		{
			range->from_value = RTSP_RANGE_TIME_NORMAL;
			range->npt.from.second = seconds;
			range->npt.from.fraction = fraction;
		}
	}
	else
	{
		range->from_value = RTSP_RANGE_TIME_NOVALUE;
		memset(&range->npt.from, 0, sizeof(range->npt.from));
	}

	assert('-' == *p);
	if('\0' == p[1] || strchr(RANGE_SPECIAL, p[1]))
	{
		assert('-' != *fields);
		range->to_value = RTSP_RANGE_TIME_NOVALUE;
		memset(&range->npt.to, 0, sizeof(range->npt.to));
	}
	else
	{
		if(!(p = rtsp_header_range_npt_time(p+1, &seconds, &fraction)))
			return -1;
		assert('\0' == p[0] || strchr(RANGE_SPECIAL, p[0]));

		range->to_value = RTSP_RANGE_TIME_NORMAL;
		range->npt.to.second = seconds;
		range->npt.to.fraction = fraction;
	}

	return 0;
}

// utc-time = utc-date "T" utc-time "Z"
// utc-date = 8DIGIT ; < YYYYMMDD >
// utc-time = 6DIGIT [ "." fraction ] ; < HHMMSS.fraction >
static const char* rtsp_header_range_clock_time(const char* str, int *year, int *month, int *day, int *second, int *fraction)
{
	const char* p;
	int hour, minute;

	assert(str);
	if(5 != sscanf(str, "%4d%2d%2dT%2d%2d", year, month, day, &hour, &minute))
		return NULL;

	p = string_token_number(str + 13, second);
	if(p && *p == '.')
	{
		p = string_token_number(str, fraction);
	}

	*second += hour*3600 * minute*60;

	assert('Z' == *p);
	assert('\0' == p[1] || strchr(RANGE_SPECIAL, p[1]));
	return 'Z'==*p ? p+1 : p;
}

// 3.7 Absolute Time (p18)
// utc-range = "clock" "=" utc-time "-" [ utc-time ]
// Range: clock=19961108T143720.25Z-
// Range: clock=19961110T1925-19961110T2015 (p72)
static int rtsp_header_range_clock(const char* fields, struct rtsp_header_range_t* range)
{
	const char* p;
	int year, month, day, second, fraction;

	p = rtsp_header_range_clock_time(fields, &year, &month, &day, &second, &fraction);
	if(!p || '-' != *p)
		return -1;

	range->from_value = RTSP_RANGE_TIME_NORMAL;
	range->clock.from.year = year;
	range->clock.from.month = month;
	range->clock.from.day = day;
	range->clock.from.second = second;
	range->clock.from.fraction = fraction;

	assert('-' == *p);
	if('\0'==p[1] || strchr(RANGE_SPECIAL, p[1]))
	{
		range->to_value = RTSP_RANGE_TIME_NOVALUE;
		memset(&range->clock.to, 0, sizeof(range->clock.to));
	}
	else
	{
		if(! (p = rtsp_header_range_clock_time(p+1, &year, &month, &day, &second, &fraction)) )
			return -1;

		range->to_value = RTSP_RANGE_TIME_NORMAL;
		range->clock.to.year = year;
		range->clock.to.month = month;
		range->clock.to.day = day;
		range->clock.to.second = second;
		range->clock.to.fraction = fraction;
	}

	return 0;
}

int rtsp_header_range(const char* fields, struct rtsp_header_range_t* range)
{
	int r = 0;
	while(fields && 0 == r)
	{
		if(0 == strnicmp("clock=", fields, 6))
		{
			range->type = RTSP_RANGE_CLOCK;
		}
		else if(0 == strnicmp("npt=", fields, 4))
		{
			range->type = RTSP_RANGE_NPT;
		}
		else if(0 == strnicmp("smpte=", fields, 6))
		{
			range->type = RTSP_RANGE_SMPTE;
			r = rtsp_header_range_smpte(fields+6, range);
		}
		else if(0 == strnicmp("smpte-30-drop=", fields, 15))
		{
			range->type = RTSP_RANGE_SMPTE_30;
			r = rtsp_header_range_smpte(fields+15, range);
		}
		else if(0 == strnicmp("smpte-25=", fields, 9))
		{
			range->type = RTSP_RANGE_SMPTE_25;
			r = rtsp_header_range_smpte(fields+9, range);
		}
		else if(0 == strnicmp("time=", fields, 5))
		{
		}
		else
		{
			fields = strchr(fields, ';');
			if(fields)
				++fields;
		}
	}

	return r;
}

#if defined(DEBUG) || defined(_DEBUG)
void rtsp_header_range_test()
{
	struct rtsp_header_range_t range;

	// smpte
	assert(0==rtsp_header_range("smpte=10:12:33:20-", &range));
	assert(range.type == RTSP_RANGE_SMPTE && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NOVALUE);
	assert(range.smpte.from.second==10*3600+12*60+33 && range.smpte.from.frame==20 && range.smpte.from.subframe==0);

	assert(0==rtsp_header_range("smpte=10:07:33-", &range));
	assert(range.type == RTSP_RANGE_SMPTE && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NOVALUE);
	assert(range.smpte.from.second==10*3600+7*60+33 && range.smpte.from.frame==0 && range.smpte.from.subframe==0);

	assert(0==rtsp_header_range("smpte=10:07:00-10:07:33:05.01", &range));
	assert(range.type == RTSP_RANGE_SMPTE && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NORMAL);
	assert(range.smpte.from.second==10*3600+7*60 && range.smpte.from.frame==0 && range.smpte.from.subframe==0);
	assert(range.smpte.to.second==10*3600+7*60+33 && range.smpte.to.frame==5 && range.smpte.to.subframe==1);

	assert(0==rtsp_header_range("smpte-25=10:07:00-10:07:33:05.01", &range));
	assert(range.type == RTSP_RANGE_SMPTE_25 && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NORMAL);
	assert(range.smpte.from.second==10*3600+7*60 && range.smpte.from.frame==0 && range.smpte.from.subframe==0);
	assert(range.smpte.to.second==10*3600+7*60+33 && range.smpte.to.frame==5 && range.smpte.to.subframe==1);

	// npt
	assert(0==rtsp_header_range("npt=now-", &range));
	assert(range.type == RTSP_RANGE_NPT && range.from_value == RTSP_RANGE_TIME_NOW && range.to_value == RTSP_RANGE_TIME_NOVALUE);

	assert(0==rtsp_header_range("npt=12:05:35.3-", &range));
	assert(range.type == RTSP_RANGE_NPT && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NOVALUE);
	assert(range.npt.from.second==12*3600+5*60+35 && range.npt.from.fraction==3);

	assert(0==rtsp_header_range("npt=123.45-125", &range));
	assert(range.type == RTSP_RANGE_NPT && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NORMAL);
	assert(range.npt.from.second==123 && range.npt.from.fraction==45);
	assert(range.npt.from.second==125 && range.npt.from.fraction==0);

	// clock
	assert(-1==rtsp_header_range("clock=-19961108T143720.25Z", &range));

	assert(0==rtsp_header_range("clock=19961108T143720.25Z-", &range));
	assert(range.type == RTSP_RANGE_CLOCK && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NOVALUE);
	assert(range.clock.from.year==1996 && range.clock.from.month==11 && range.clock.from.day==8 && range.clock.from.second==14*3600+37*60+20 && range.clock.from.fraction==25);

	assert(0==rtsp_header_range("clock=19961110T1925-19961110T2015", &range)); // rfc2326 (p72)
	assert(range.type == RTSP_RANGE_CLOCK && range.from_value == RTSP_RANGE_TIME_NORMAL && range.to_value == RTSP_RANGE_TIME_NORMAL);
	assert(range.clock.from.year==1996 && range.clock.from.month==11 && range.clock.from.day==10 && range.clock.from.second==19*3600+25*60 && range.clock.from.fraction==0);
	assert(range.clock.to.year==1996 && range.clock.to.month==11 && range.clock.to.day==10 && range.clock.from.second==20*3600+15*60 && range.clock.from.fraction==0);
}
#endif
