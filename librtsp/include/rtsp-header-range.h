#ifndef _rtsp_header_range_h_
#define _rtsp_header_range_h_

#include "ctypedef.h"

struct time_smpte_t
{
	int second;		// [0,24*60*60)
	int frame;		// [0,99]
	int subframe;	// [0,99]
};

struct time_npt_t
{
	int64_t second;	// [0,24*60*60)
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

enum ERTSP_RANGE_TIME { 
	RTSP_RANGE_SMPTE = 1, 
	RTSP_RANGE_SMPTE_30=RTSP_RANGE_SMPTE, 
	RTSP_RANGE_SMPTE_25, 
	RTSP_RANGE_NPT, 
	RTSP_RANGE_CLOCK,
};

enum ERTSP_RANGE_TIME_VALUE { 
	RTSP_RANGE_TIME_NORMAL = 1, 
	RTSP_RANGE_TIME_NOW, // npt now
	RTSP_RANGE_TIME_NOVALUE, // npt don't set from value: -[npt-time]
};

struct rtsp_header_range_t
{
	enum ERTSP_RANGE_TIME type;

	union 
	{
		struct range_smpte_t
		{
			struct time_smpte_t from;
			struct time_smpte_t to;
		} smpte;

		struct range_npt_t
		{
			struct time_npt_t from;
			struct time_npt_t to;
		} npt;

		struct range_clock_t
		{
			struct time_clock_t from;
			struct time_clock_t to;
		} clock;
	};

	enum ERTSP_RANGE_TIME_VALUE from_value;
	enum ERTSP_RANGE_TIME_VALUE to_value;
};

#endif /* !_rtsp_header_range_h_ */
