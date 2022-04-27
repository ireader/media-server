#include "dash-parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>

const char* dash_segment_initialization(const struct dash_segment_t* segment);
int dash_segment_count(const struct dash_segment_t* segment);
int dash_segment_find(const struct dash_segment_t* segment, int64_t time);
int dash_segment_information(const struct dash_segment_t* segment, int index, int64_t* number, int64_t* start, int64_t* duration, const char** url, const char** range);

const struct dash_url_t* dash_representation_get_base_url(const struct dash_representation_t* representation)
{
	const struct dash_mpd_t* mpd;
	const struct dash_period_t* period;
	const struct dash_adaptation_set_t* set;

	if (representation->base_urls.count > 0)
		return &representation->base_urls;

	set = representation->parent;
	if (set->base_urls.count > 0)
		return &set->base_urls;

	period = set->parent;
	if (period->base_urls.count > 0)
		return &period->base_urls;

	mpd = period->parent;
	return &mpd->urls;
}

const struct dash_segment_t* dash_representation_get_segment(const struct dash_representation_t* representation)
{
	const struct dash_period_t* period;
	const struct dash_adaptation_set_t* set;

	if (DASH_SEGMENT_NONE != representation->segment.type)
		return &representation->segment;

	set = representation->parent;
	if (DASH_SEGMENT_NONE != set->segment.type)
		return &set->segment;

	period = set->parent;
	if (DASH_SEGMENT_NONE != period->segment.type)
		return &period->segment;

	return &representation->segment;
}

/// @return >=0-ok with length, <0-error
static int dash_representation_template_replace(const struct dash_representation_t* representation, const char* url, int64_t number, int64_t start, char* ptr, size_t len)
{
	// Each identifier may be suffixed, within the enclosing '$' characters, 
	// with an additional format tag aligned with the printf format tag:
	//	%0[width]d
	//const char* patterns[] = { "RepresentationID", "Number", "Bandwidth", "Time", "SubNumber" };
	size_t i, j;
	int width, off;
	char format[16];

	for (j = i = 0; i < strlen(url) && j < len; i++)
	{
		if ('$' == url[i])
		{
			off = 0;
			width = 1;

			// Identifier matching is case-sensitive.
			if ('$' == url[i + 1])
			{
				// skip
				ptr[j++] = url[i++];
				continue;
			}
			else if (0 == strncmp("$RepresentationID$", url + i, 18))
			{
				j += snprintf(ptr + j, j < len ? len - j : 0, "%s", representation->id ? representation->id : "");
				i += 17;
			}
			else if (0 == strncmp("$Number", url + i, 7) && ('$' == url[i + 7] || ('%' == url[i + 7] && 1 == sscanf(url + i + 7 + 1, "%dd$%n", &width, &off) && '$' == url[i + 7 + off])))
			{
				snprintf(format, sizeof(format) - 1, "%%0%d" PRId64, width);
				j += snprintf(ptr + j, j < len ? len - j : 0, format, number);
				i += 7 + off;
			}
			else if (0 == strncmp("$Bandwidth", url + i, 10) && ('$' == url[i + 10] || ('%' == url[i + 10] && 1 == sscanf(url + i + 10 + 1, "%dd$%n", &width, &off) && '$' == url[i + 10 + off])))
			{
				snprintf(format, sizeof(format) - 1, "%%0%du", width);
				j += snprintf(ptr + j, j < len ? len - j : 0, format, representation->bandwidth);
				i += 10 + off;
			}
			else if (0 == strncmp("$Time", url + i, 5) && ('$' == url[i + 5] || ('%' == url[i + 5] && 1 == sscanf(url + i + 5 + 1, "%dd$%n", &width, &off) && '$' == url[i + 5 + off])))
			{
				snprintf(format, sizeof(format) - 1, "%%0%d" PRId64, width);
				j += snprintf(ptr + j, j < len ? len - j : 0, format, start);
				i += 5 + off;
			}
			else if (0 == strncmp("$SubNumber", url + i, 10) && ('$' == url[i + 10] || '%' == url[i + 10]))
			{
				// TODO:
				assert(0);
			}
			else
			{
				assert(0); // ignore
				ptr[j++] = url[i];
			}
		}
		else
		{
			ptr[j++] = url[i];
		}
	}

	if (j < len)
		ptr[j] = '\0';
	return j < len ? (int)j : -1;
}

int dash_representation_get_initialization(const struct dash_representation_t* representation, char* url, size_t size)
{
	const char* ptr;
	const struct dash_period_t* period;
	const struct dash_adaptation_set_t* set;

	ptr = dash_segment_initialization(&representation->segment);
	if (!ptr && representation->parent)
	{
		set = representation->parent;
		ptr = dash_segment_initialization(&set->segment);
		if (!ptr && set->parent)
		{
			period = set->parent;
			ptr = dash_segment_initialization(&period->segment);
			if (!ptr)
				return 0;
		}
	}

	return dash_representation_template_replace(representation, ptr, 0, 0, url, size);
}

int dash_representation_segment_url(const struct dash_representation_t* representation, int index, int64_t* number, int64_t* start, int64_t* duration, const char** range, char* url, size_t size)
{
	int r;
	const struct dash_segment_t* segment;
	const char* url0;
	
	segment = dash_representation_get_segment(representation);
	assert(index >= 0 && index < dash_segment_count(segment));
	r = dash_segment_information(segment, index, number, start, duration, &url0, range);
	if (r < 0)
		return r;

	return dash_representation_template_replace(representation, url0, *number, *start, url, size);
}

int dash_representation_find_segment(const struct dash_representation_t* representation, int64_t time)
{
	const struct dash_segment_t* segment;
	segment = dash_representation_get_segment(representation);
	return dash_segment_find(segment, time);
}

int dash_representation_segment_count(const struct dash_representation_t* representation)
{
	const struct dash_segment_t* segment;
	segment = dash_representation_get_segment(representation);
	return dash_segment_count(segment);
}

#if defined(_DEBUG) || defined(DEBUG)
void dash_representation_test(void)
{
	char ptr[128];
	struct dash_representation_t r;
	memset(&r, 0, sizeof(r));
	r.id = "0";
	r.bandwidth = 19200;
	assert(23 == dash_representation_template_replace(&r, "dash-$$$$$RepresentationID$$Bandwidth$$Number$$Time$-", 1, 19700101, ptr, sizeof(ptr)));
	assert(0 == strcmp(ptr, "dash-$$019200119700101-"));
	assert(22 == dash_representation_template_replace(&r, "dash-$Bandwidth%03d$$Number%03d$$Time%03d$-", 1, 19700101, ptr, sizeof(ptr)));
	assert(0 == strcmp(ptr, "dash-1920000119700101-"));
}
#endif
