#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

static const uint8_t s_base16_dec[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, /* 0 - 9 */
	0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A - F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* a - f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

size_t hls_base16_decode(void* target, const char* source, size_t bytes)
{
	size_t i;
	uint8_t* p;
	p = (uint8_t*)target;
	assert(0 == bytes % 2);
	for (i = 0; i < bytes / 2; i++)
	{
		p[i] = s_base16_dec[(unsigned char)source[i * 2]] << 4;
		p[i] |= s_base16_dec[(unsigned char)source[i * 2 + 1]];
	}
	return i;
}

const char* hls_strtrim(const char* s, size_t* n, const char* prefix, const char* suffix)
{
	while (s && *n > 0 && prefix && strchr(prefix, *s))
	{
		--* n;
		++s;
	}

	while (s && *n > 0 && suffix && strchr(suffix, s[*n - 1]))
		--* n;

	return s;
};

size_t hls_strsplit(const char* ptr, const char* end, const char* delimiters, const char* quotes, const char** ppnext)
{
	char q;
	const char* p;

	assert(end && delimiters);
	q = 0;
	for (p = ptr; p && *p && p < end; p++)
	{
		if (q)
		{
			// find QUOTES first
			if (q == *p)
			{
				q = 0;
				continue;
			}
		}
		else
		{
			if (strchr(delimiters, *p))
			{
				break;
			}
			else if (quotes && strchr(quotes, *p))
			{
				q = *p;
			}
		}
	}

	if (ppnext)
	{
		*ppnext = p;
		while (*ppnext && *ppnext < end && strchr(delimiters, **ppnext))
			++* ppnext;
	}

	return p - ptr;
}
