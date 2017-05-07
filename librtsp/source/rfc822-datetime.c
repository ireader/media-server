#include "rfc822-datetime.h"
#include <stdio.h>
#include <assert.h>
#include <time.h>

// Tue, 15 Nov 1994 08:12:31 GMT
// 23 Jan 1997 15:35:06 GMT
/*
// RFC822 
// 5.  DATE AND TIME SPECIFICATION
date-time   =  [ day "," ] date time        ; dd mm yy
											;  hh:mm:ss zzz

day         =  "Mon"  / "Tue" /  "Wed"  / "Thu"
			/  "Fri"  / "Sat" /  "Sun"

date        =  1*2DIGIT month 2DIGIT        ; day month year
											;  e.g. 20 Jun 82

month       =  "Jan"  /  "Feb" /  "Mar"  /  "Apr"
			/  "May"  /  "Jun" /  "Jul"  /  "Aug"
			/  "Sep"  /  "Oct" /  "Nov"  /  "Dec"

time        =  hour zone                    ; ANSI and Military

hour        =  2DIGIT ":" 2DIGIT [":" 2DIGIT]
											; 00:00:00 - 23:59:59

zone        =  "UT"  / "GMT"                ; Universal Time
											; North American : UT
			/  "EST" / "EDT"                ;  Eastern:  - 5/ - 4
			/  "CST" / "CDT"                ;  Central:  - 6/ - 5
			/  "MST" / "MDT"                ;  Mountain: - 7/ - 6
			/  "PST" / "PDT"                ;  Pacific:  - 8/ - 7
			/  1ALPHA                       ; Military: Z = UT;
											;  A:-1; (J not used)
											;  M:-12; N:+1; Y:+12
			/ ( ("+" / "-") 4DIGIT )        ; Local differential
											;  hours+min. (HHMM)
*/

static const char* s_month[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char* s_week[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char* s_zone[] = {
	"UTC", "GMT", "EST", "EDT", "CST", "CDT",
	"MST", "MDT", "PST", "PDT"
};

const char* rfc822_datetime_format(time_t time, rfc822_datetime_t datetime)
{
	int r;
	struct tm *tm = gmtime(&time);
	assert(0 <= tm->tm_wday && tm->tm_wday < 7);
	assert(0 <= tm->tm_mon && tm->tm_mon < 12);
	assert(sizeof(rfc822_datetime_t) >= 30);
	r = snprintf(datetime, sizeof(rfc822_datetime_t), "%s, %02d %s %04d %02d:%02d:%02d GMT",
		s_week[(unsigned int)tm->tm_wday % 7],
		tm->tm_mday,
		s_month[(unsigned int)tm->tm_mon % 12],
		tm->tm_year+1900,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec);
	return r > 0 && r < sizeof(rfc822_datetime_t) ? datetime : NULL;
}

//time_t datetime_parse(const char* datetime)
//{
//	return 0;
//}
