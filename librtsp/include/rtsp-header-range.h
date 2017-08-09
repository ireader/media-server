#ifndef _rtsp_header_range_h_
#define _rtsp_header_range_h_

#include <stdint.h>

enum ERTSP_RANGE_TIME { 
	RTSP_RANGE_SMPTE = 1, // relative to the start of the clip 
	RTSP_RANGE_SMPTE_30=RTSP_RANGE_SMPTE, 
	RTSP_RANGE_SMPTE_25, 
	RTSP_RANGE_NPT,  // relative to the beginning of the presentation
	RTSP_RANGE_CLOCK, // absolute time, ISO 8601 timestamps, UTC(GMT)
};

enum ERTSP_RANGE_TIME_VALUE { 
	RTSP_RANGE_TIME_NORMAL = 1, 
	RTSP_RANGE_TIME_NOW, // npt now
	RTSP_RANGE_TIME_NOVALUE, // npt don't set from value: -[npt-time]
};

struct rtsp_header_range_t
{
	enum ERTSP_RANGE_TIME type;
	enum ERTSP_RANGE_TIME_VALUE from_value;
	enum ERTSP_RANGE_TIME_VALUE to_value;

	uint64_t from; // ms
	uint64_t to; // ms

	uint64_t time; // range time parameter(in ms), 0 if no value
};

/// parse RTSP Range header
/// @return 0-ok, other-error
/// usage 1:
/// struct rtsp_header_range_t range;
/// const char* header = "Range: clock=19960213T143205Z-;time=19970123T143720Z";
/// r = rtsp_header_range("clock=19960213T143205Z-;time=19970123T143720Z", &range);
/// check(r)
/// 
/// usage 2:
/// const char* header = "Range: smpte-25=10:07:00-10:07:33:05.01,smpte-25=11:07:00-11:07:33:05.01";
/// split(header, ',');
/// r1 = rtsp_header_range("smpte-25=10:07:00-10:07:33:05.01", &range);
/// r2 = rtsp_header_range("smpte-25=11:07:00-11:07:33:05.01", &range);
/// check(r1, r2)
int rtsp_header_range(const char* field, struct rtsp_header_range_t* range);

#endif /* !_rtsp_header_range_h_ */
