#include "dash-mpd.h"
#include "dash-proto.h"
#include "mov-format.h"
#include "fmp4-writer.h"
#include "list.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64) || defined(OS_WINDOWS)
	#define PRId64 "I64d"
#endif

#define N_TRACK 8
#define N_NAME 128
#define N_COUNT 3

struct dash_segment_t
{
	struct list_head link;
	char* name;
	int64_t timestamp;
	int64_t duration;
};

struct dash_adaptation_set_t
{
	fmp4_writer_t* fmp4;
	int64_t dts;
	int64_t dts_last;
	int track;
	
	int seq;
	int id;
	uint8_t object;

	union
	{
		struct
		{
			int width;
			int height;
			int frame_rate;
			int bitrate;
			struct
			{
				uint8_t profile;
				uint8_t compatibility;
				uint8_t level;
			} avc;
		} video;

		struct
		{
			int channel;
			int sample_bit;
			int sample_rate;
			int bitrate;
		} audio;
	} u;

	size_t count;
	struct list_head root; // segments
};

struct dash_mpd_t
{
	int flags;
	char* name;
	time_t time;

	struct dash_mpd_notify_t notify;
	void* param;

	size_t count; // adaptation set count
	struct dash_adaptation_set_t tracks[N_TRACK];
};

static int dash_adaptation_set_open(struct dash_mpd_t* mpd, struct dash_adaptation_set_t* track)
{
	char name[N_NAME + 16];
	snprintf(name, sizeof(name), "%s-%d-%" PRId64 ".mp4", mpd->name, track->id, track->dts);

	track->fmp4 = fmp4_writer_create(name, MOV_FLAG_SEGMENT);
	if (NULL == track->fmp4)
		return -1; // error create mp4 file

	if (MOV_OBJECT_H264 == track->object)
	{
		track->track = fmp4_writer_add_video(track->fmp4, track->object, track->u.video.width, track->u.video.height, NULL, 0);
	}
	else if (MOV_OBJECT_AAC == track->object)
	{
		track->track = fmp4_writer_add_audio(track->fmp4, track->object, track->u.audio.channel, track->u.audio.sample_bit, track->u.audio.sample_rate, NULL, 0);
	}
	else
	{
		assert(0);
	}

	return 0;
}

static int dash_adaptation_set_close(struct dash_mpd_t* mpd, struct dash_adaptation_set_t* track)
{
	size_t len;
	char name[N_NAME + 16];
	struct list_head *link;
	struct dash_segment_t* seg;

	if (track->fmp4)
	{
		fmp4_writer_destroy(track->fmp4);
		track->fmp4 = NULL;

		len = snprintf(name, sizeof(name), "%s-%d-%d.mp4", mpd->name, track->id, track->seq);
		seg = (struct dash_segment_t*)malloc(sizeof(*seg) + len + 1);
		seg->name = (char*)(seg + 1);
		seg->timestamp = track->dts;
		seg->duration = track->dts_last - track->dts;
		memcpy(seg->name, name, len + 1);

		list_insert_after(&seg->link, track->root.prev);
		track->seq += 1;

		track->count += 1;
		if (DASH_DYNAMIC == mpd->flags && track->count > N_COUNT)
		{
			link = track->root.next;
			list_remove(link);
			seg = list_entry(link, struct dash_segment_t, link);
			free(seg);
		}
	}
	return 0;
}

struct dash_mpd_t* dash_mpd_create(const char* name, int flags, struct dash_mpd_notify_t* notify, void* param)
{
	size_t len;
	struct dash_mpd_t* mpd;

	len = strlen(name ? name : "") + 1;
	if (len < 2 || len > N_NAME)
		return NULL; // too big/empty

	mpd = (struct dash_mpd_t*)calloc(1, sizeof(*mpd) + len);
	if (mpd)
	{
		mpd->flags = flags;
		mpd->name = (char*)(mpd + 1);
		memcpy(mpd->name, name, len);
		memcpy(&mpd->notify, notify, sizeof(mpd->notify));
		mpd->param = param;
		mpd->time = time(NULL);
	}
	return mpd;
}

void dash_mpd_destroy(struct dash_mpd_t* mpd)
{
	size_t i;
	struct list_head *p, *n;
	struct dash_segment_t *seg;
	struct dash_adaptation_set_t* track;

	for (i = 0; i < mpd->count; i++)
	{
		track = &mpd->tracks[i];
		dash_adaptation_set_close(mpd, track);

		list_for_each_safe(p, n, &track->root)
		{
			seg = list_entry(p, struct dash_segment_t, link);
			free(seg);
		}
	}

	free(mpd);
}

int dash_mpd_add_video_adapation_set(struct dash_mpd_t* mpd, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size)
{
	char name[N_NAME + 16];
	fmp4_writer_t* fmp4;
	struct dash_adaptation_set_t* track;
	if (mpd->count + 1 >= N_TRACK)
		return -1;

	track = &mpd->tracks[mpd->count++];
	LIST_INIT_HEAD(&track->root);
	track->id = mpd->count;
	track->object = object;
	track->u.video.width = width;
	track->u.video.height = height;
	track->u.video.frame_rate = 25;
	track->u.video.bitrate = 0;
	assert(((const uint8_t*)extra_data)[0] == 1); // configurationVersion
	track->u.video.avc.profile = ((const uint8_t*)extra_data)[1];
	track->u.video.avc.compatibility = ((const uint8_t*)extra_data)[2];
	track->u.video.avc.level = ((const uint8_t*)extra_data)[3];

	assert(MOV_OBJECT_H264 == object);
	snprintf(name, sizeof(name), "%s-%d-init.mp4", mpd->name, track->id);
	fmp4 = fmp4_writer_create(name, 0);
	if (fmp4)
	{
		fmp4_writer_add_video(fmp4, object, width, height, extra_data, extra_data_size);
		fmp4_writer_destroy(fmp4); // save init segment file
	}
	else
	{
		--mpd->count;
	}
	return fmp4 ? track->id : -1;
}

int dash_mpd_add_audio_adapation_set(struct dash_mpd_t* mpd, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
{
	char name[N_NAME + 16];
	fmp4_writer_t* fmp4;
	struct dash_adaptation_set_t* track;
	if (mpd->count + 1 >= N_TRACK)
		return -1;

	track = &mpd->tracks[mpd->count++];
	LIST_INIT_HEAD(&track->root);
	track->id = mpd->count;
	track->object = object;
	track->u.audio.channel = channel_count;
	track->u.audio.sample_bit = bits_per_sample;
	track->u.audio.sample_rate = sample_rate;
	track->u.audio.bitrate = 0;

	assert(MOV_OBJECT_AAC == object);
	snprintf(name, sizeof(name), "%s-%d-init.mp4", mpd->name, track->id);
	fmp4 = fmp4_writer_create(name, 0);
	if (fmp4)
	{
		fmp4_writer_add_audio(fmp4, object, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size);
		fmp4_writer_destroy(fmp4); // save init segment file
	}
	else
	{
		--mpd->count;
	}
	return fmp4 ? track->id : -1;
}

int dash_mpd_input(struct dash_mpd_t* mpd, int adapation, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	int r;
	size_t i;
	struct dash_adaptation_set_t* track;
	if (adapation > N_TRACK || adapation < 1)
		return -1;

	track = &mpd->tracks[adapation - 1];
	if (NULL == data || 0 == bytes // flash fragment
		|| (MOV_OBJECT_H264 == track->object && (MOV_AV_FLAG_KEYFREAME & flags)))
	{
		for (i = 0; i < mpd->count; i++)
		{
			dash_adaptation_set_close(mpd, &mpd->tracks[i]);
		}

		// FIXME: check count(first time only)
		if(mpd->notify.onupdate)
			mpd->notify.onupdate(mpd->param); // notify update
	}

	if (NULL == data || 0 == bytes)
		return 0;

	r = 0;
	if (NULL == track->fmp4)
	{
		track->dts = dts;
		r = dash_adaptation_set_open(mpd, track);
	}

	track->dts_last = dts;
	return 0 == r ? fmp4_writer_write(track->fmp4, track->id, data, bytes, pts, dts, flags) : r;
}

// ISO/IEC 23009-1:2014(E) 5.4 Media Presentation Description updates (p67)
// 1. the value of MPD@id, if present, shall be the same in the original and the updated MPD;
// 2. the values of any Period@id attributes shall be the same in the original and the updated MPD, unless the containing Period element has been removed;
// 3. the values of any AdaptationSet@id attributes shall be the same in the original and the updated MPD unless the containing Period element has been removed;
size_t dash_mpd_playlist(struct dash_mpd_t* mpd, char* playlist, size_t bytes)
{
	// ISO/IEC 23009-1:2014(E)
	// G.2 Example for ISO Base media file format Live profile (141)
	static const char* s_mpd_live =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<MPD\n"
		"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
		"    xmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
		"    xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\"\n"
		"    type=\"dynamic\"\n"
		"    minimumUpdatePeriod=\"PT%uS\"\n"
		"    timeShiftBufferDepth=\"PT%uS\"\n"
		"    availabilityStartTime=\"%s\"\n"
		"    minBufferTime=\"PT%uS\"\n"
		"    publishTime=\"%s\"\n"
		"    profiles=\"urn:mpeg:dash:profile:isoff-live:2011\">\n";

	// ISO/IEC 23009-1:2014(E)
	// G.1 Example MPD for ISO Base media file format On Demand profile
	static const char* s_mpd_static =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<MPD\n"
		"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
		"    xmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
		"    xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\"\n"
		"    type=\"static\"\n"
		"    mediaPresentationDuration=\"PT%uS\"\n"
		"    minBufferTime=\"PT%uS\"\n"
		"    profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\">\n";

	static const char* s_video =
		"    <AdaptationSet id=\"1\" mimeType=\"video/mp4\" segmentAlignment=\"true\">\n"
		"      <Representation id=\"H264\" codecs=\"avc1.%02x%02x%02x\" width=\"%d\" height=\"%d\" frameRate=\"%d\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
		"        <SegmentTemplate timescale=\"1000\" media=\"%s-%d-$Time$.mp4\" initialization=\"%s-%d-init.mp4\">\n"
		"          <SegmentTimeline>\n";

	static const char* s_audio =
		"    <AdaptationSet id=\"2\" mimeType=\"audio/mp4\" segmentAlignment=\"true\">\n"
		"      <AudioChannelConfiguration  schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"1\"/>\n"
		"      <Representation id=\"AAC\" codecs=\"mp4a.40.5\" audioSamplingRate=\"%d\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
		"        <SegmentTemplate timescale=\"1000\" media=\"%s-%d-$Time$.mp4\" initialization=\"%s-%d-init.mp4\">\n"
		"          <SegmentTimeline>\n";

	static const char* s_footer =
		"          </SegmentTimeline>\n"
		"        </SegmentTemplate>\n"
		"      </Representation>\n"
		"    </AdaptationSet>\n";

	size_t i, n;
	time_t now;
	struct tm* t;
	char publishTime[32];
	char availabilityStartTime[32];
	unsigned int minimumUpdatePeriod = 4;
	struct dash_adaptation_set_t* track;
	struct dash_segment_t *seg;
	struct list_head *link;

	t = gmtime(&mpd->time);
	snprintf(availabilityStartTime, sizeof(availabilityStartTime), "%4d-%02d-%02dT%02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	
	now = time(NULL);
	t = gmtime(&now);
	snprintf(publishTime, sizeof(publishTime), "%4d-%02d-%02dT%02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	
	/*
	* timeShiftBufferDepth formula:
	*     2 * minBufferTime + max_fragment_length + 1
	*/
	n = snprintf(playlist, bytes, s_mpd_live, minimumUpdatePeriod, minimumUpdatePeriod * 3 + 1, availabilityStartTime, minimumUpdatePeriod, publishTime);
	n += snprintf(playlist + n, bytes - n, "  <Period start=\"PT0S\" id=\"dash\">\n");

	for (i = 0; i < mpd->count; i++)
	{
		track = &mpd->tracks[i];
		if (MOV_OBJECT_H264 == track->object)
		{
			n += snprintf(playlist + n, bytes - n, s_video, (unsigned int)track->u.video.avc.profile, (unsigned int)track->u.video.avc.compatibility, (unsigned int)track->u.video.avc.level, track->u.video.width, track->u.video.height, track->u.video.frame_rate, track->u.video.bitrate, mpd->name, track->id, mpd->name, track->id);
			list_for_each(link, &track->root)
			{
				seg = list_entry(link, struct dash_segment_t, link);
				n += snprintf(playlist + n, bytes - n, "             <S t=\"%" PRId64 "\" d=\"%u\"/>\n", seg->timestamp, (unsigned int)seg->duration);
			}
			n += snprintf(playlist + n, bytes - n, s_footer);
		}
		else if (MOV_OBJECT_AAC == track->object)
		{
			n += snprintf(playlist + n, bytes - n, s_audio, track->u.audio.sample_rate, track->u.audio.bitrate, mpd->name, track->id, mpd->name, track->id);
			list_for_each(link, &track->root)
			{
				seg = list_entry(link, struct dash_segment_t, link);
				n += snprintf(playlist + n, bytes - n, "             <S t=\"%" PRId64 "\" d=\"%u\"/>\n", seg->timestamp, (unsigned int)seg->duration);
			}
			n += snprintf(playlist + n, bytes - n, s_footer);
		}
		else
		{
			assert(0);
		}
	}

	n += snprintf(playlist + n, bytes - n, "  </Period>\n</MPD>\n");
	return n;
}
