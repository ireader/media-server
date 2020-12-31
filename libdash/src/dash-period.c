#include "dash-parser.h"
#include "hls-string.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int64_t dash_get_duration(const struct dash_mpd_t* mpd)
{
	size_t i;
	int64_t t, start;
	const struct dash_period_t* period;

	if (DASH_DYNAMIC == mpd->type)
		return -1;

	if (mpd->media_presentation_duration > 0)
		return mpd->media_presentation_duration;

	t = 0;
	for (i = 0; i < mpd->period_count; i++)
	{
		period = &mpd->periods[i];
		if (period->start > 0)
		{
			// a regular Period or an early terminated Period
			start = period->start;
		}
		else if (i > 0 && mpd->periods[i - 1].duration > 0)
		{
			start = t;
		}
		else if (0 == i && DASH_STATIC == mpd->type)
		{
			start = 0;
		}
		else if ((0 == i || mpd->periods[i - 1].duration == 0) && DASH_DYNAMIC == mpd->type)
		{
			continue; // Early Available Period
		}
		else
		{
			start = t;
		}

		if (period->duration > 0)
		{
			t = start + period->duration;
		}
		else if (i + 1 < mpd->period_count)
		{
			assert(0 != mpd->periods[i + 1].start);
			t = mpd->periods[i + 1].start;
		}
		else if (DASH_DYNAMIC == mpd->type)
		{
			// MPD@mediaPresentationDuration shall be present when neither 
			// the attribute MPD@minimumUpdatePeriod nor the Period@duration 
			// of the last Period are present.
			assert(mpd->media_presentation_duration > 0 || mpd->minimum_update_period > 0);
			t = mpd->media_presentation_duration > 0 ? mpd->media_presentation_duration : (start + mpd->minimum_update_period);
		}
		else
		{
			t = start; // ???
		}
	}

	return t;
}

int dash_period_find(const struct dash_mpd_t* mpd, int64_t time)
{
	size_t i;
	int64_t t, start;
	const struct dash_period_t* period;

	if (1 == mpd->period_count)
		return 0; // only one period

	t = 0;
	for (i = 0; i < mpd->period_count; i++)
	{
		// 5.3.2.1 Overview (p27)
		// 1. If the attribute @start is present in the Period, then the Period 
		//    is a regular Period or an early terminated Period and the PeriodStart 
		//    is equal to the value of this attribute.
		// 2. If the @start attribute is absent, but the previous Period element 
		//    contains a @duration attributethen this new Period is also a regular 
		//    Period or an early terminated Period. The start time of the new Period 
		//    PeriodStart is the sum of the start time of the previous Period PeriodStart 
		//    and the value of the attribute @duration of the previous Period.
		// 3. If (i) @start attribute is absent, and (ii) the Period element is the 
		//    first in the MPD, and (iii) the MPD@type is 'static', then the PeriodStart 
		//    time shall be set to zero
		// 4. If (i) @start attribute is absent, and (ii) the previous Period element does 
		//    not contain a @durationattribute or the Period element is the first in the 
		//    MPD, and (iii) the MPD@type is 'dynamic', then thisPeriod is an Early Available 
		//    Period (see below for details)
		// 5. If (i) @duration attribute is present, and (ii) the next Period element contains 
		//    a @start attribute orthe @minimumUpdatePeriod is present, then this Period 
		//    is an Early Terminated Period (see below for details)
		
		period = &mpd->periods[i];
		if (period->start > 0)
		{
			// a regular Period or an early terminated Period
			start = period->start;
		}
		else if (i > 0 && mpd->periods[i - 1].duration > 0)
		{
			start = t;
		}
		else if (0 == i && DASH_STATIC == mpd->type)
		{
			start = 0;
		}
		else if( (0 == i || mpd->periods[i - 1].duration == 0) && DASH_DYNAMIC == mpd->type)
		{
			continue; // Early Available Period
		}
		else
		{
			start = t;
		}

		if (period->duration > 0)
		{
			t = start + period->duration;
		}
		else if (i + 1 < mpd->period_count)
		{
			assert(0 != mpd->periods[i + 1].start);
			t = mpd->periods[i + 1].start;
		}
		else if(DASH_DYNAMIC == mpd->type)
		{
			// MPD@mediaPresentationDuration shall be present when neither 
			// the attribute MPD@minimumUpdatePeriod nor the Period@duration 
			// of the last Period are present.
			assert(mpd->media_presentation_duration > 0 || mpd->minimum_update_period > 0);
			t = mpd->media_presentation_duration > 0 ? mpd->media_presentation_duration : (start + mpd->minimum_update_period);
		}
		else
		{
			t = start; // ???
		}

		if (time < t)
			return (int)i;
	}

	return -1; // not found
}

const struct dash_adaptation_set_t* dash_period_select(const struct dash_period_t* period, int media, unsigned int id, unsigned int group, const char* lang, const char* codecs)
{
	size_t i;
	const struct dash_adaptation_set_t* set;

	for (i = 0; i < period->adaptation_set_count; i++)
	{
		set = &period->adaptation_sets[i];
		if (DASH_MEDIA_UNKNOWN != media && media != dash_adaptation_set_media_type(set))
			continue;

		if (0 != id && id != set->id)
			continue;

		if (0 != group && group != set->group)
			continue;

		if (lang && *lang && set->lang && 0 != strcasecmp(set->lang, lang))
			continue;

		if (codecs && *codecs && set->base.codecs && 0 != strncasecmp(set->base.codecs, codecs, strlen(codecs)))
			continue;
		
		return set;
	}

	return NULL;
}
