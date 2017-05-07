#ifndef _rfc822_datetime_h_
#define _rfc822_datetime_h_

#include <time.h>

typedef char rfc822_datetime_t[30];

const char* rfc822_datetime_format(time_t time, rfc822_datetime_t datetime);

#endif /* !_rfc822_datetime_h_ */
