#ifndef _rtsp_util_h_
#define _rtsp_util_h_

#include "cstringext.h"

inline const char* string_token_word(const char* str, const char* escape)
{
	while(*str && !strchr(escape, *str))
		++str;
	return str;
}

inline const char* string_token_number(const char* str, int *value)
{
	*value = 0;
	while('0' <= *str && *str <= '9')
	{
		*value = (*value * 10) + (*str - '0');
		++str;
	}
	return str;
}

inline const char* string_token_int64(const char* str, int64_t *value)
{
	*value = 0;
	while('0' <= *str && *str <= '9')
	{
		*value = (*value * 10) + (*str - '0');
		++str;
	}
	return str;
}

#endif /* !_rtsp_util_h_ */
