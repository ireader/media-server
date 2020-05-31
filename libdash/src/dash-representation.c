#include "dash-parser.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

int dash_representation_find(const struct dash_representation_t* representation, int64_t start)
{
	int r, n, i, mid;
	int64_t number, t, d;
	const char* url, * range;
	const struct dash_segment_t* s;

	s = &representation->segment;
	n = dash_segment_count(s);
	i = 0;
	mid = -1;

	while (i < n)
	{
		mid = (i + n) / 2;
		r = dash_segment_information(s, mid, &number, &t, &d, &url, &range);
		if (0 != r)
			return r;

		if (start < t)
			n = mid;
		else if (start > t + d)
			i = mid + 1;
		else
			break;
	}

	return mid;
}
