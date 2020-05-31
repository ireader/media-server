#include "dash-parser.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cstringext.h"

const struct dash_period_t* dash_period_find(const struct dash_mpd_t* mpd, int64_t now)
{
	size_t i;
	int64_t t, start;
	const struct dash_period_t* period;

	t = 0;
	for (i = 0; i < mpd->period_count; i++)
	{
		// 5.3.2.1 Overview (p27)
		period = &mpd->periods[i];
		if (period->start > 0)
		{
			// a regular Period or an earlyterminated Period
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

		if (start <= now && now <= t)
			return period;
	}

	return mpd->period_count > 0 ? &mpd->periods[mpd->period_count - 1] : NULL;
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
