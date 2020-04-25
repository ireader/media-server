// https://en.wikipedia.org/wiki/ISO_8601
// Durations: 
// 1. P[n]Y[n]M[n]DT[n]H[n]M[n]S
// 2. P[n]W
// 3. P<date>T<time>
// 4. PYYYYMMDDThhmmss
// 5. P[YYYY]-[MM]-[DD]T[hh]:[mm]:[ss]
// For example, "P3Y6M4DT12H30M5S" represents a duration of "three years, six months, four days, twelve hours, thirty minutes, and five seconds".
// "P23DT23H" and "P4Y" "P0.5Y" == "P0,5Y"
// "PT0S" or "P0D"
// "P0003-06-04T12:30:05"

#include "dash-mpd.h"
#include "dash-proto.h"
#include "mov-format.h"
#include "fmp4-writer.h"
#include "list.h"
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#define N_TRACK 8
#define N_NAME 128
#define N_COUNT 5

#define N_SEGMENT (1 * 1024 * 1024)
#define N_FILESIZE (100 * 1024 * 1024) // 100M

#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct dash_segment_t
{
	struct list_head link;
	int64_t timestamp;
	int64_t duration;
};

struct dash_adaptation_set_t
{
	fmp4_writer_t* fmp4;
	char prefix[N_NAME];

	uint8_t* ptr;
	size_t bytes;
	size_t capacity;
	size_t offset;
	size_t maxsize; // max bytes per mp4 file

	int64_t pts;
	int64_t dts;
	int64_t dts_last;
	int64_t raw_bytes;
	int bitrate;
	int track; // MP4 track id
	int setid; // dash adapation set id
	
	int seq;
	uint8_t object;

	union
	{
		struct
		{
			int width;
			int height;
			int frame_rate;
			struct
			{
				uint8_t profile;
				uint8_t compatibility;
				uint8_t level;
			} avc;
		} video;

		struct
		{
			uint8_t profile; // AAC profile
			int channel;
			int sample_bit;
			int sample_rate;
		} audio;
	} u;

	size_t count;
	struct list_head root; // segments
};

struct dash_mpd_t
{
	int flags;
	time_t time;
	int64_t duration;
	int64_t max_segment_duration;

	dash_mpd_segment handler;
	void* param;

	int count; // adaptation set count
	struct dash_adaptation_set_t tracks[N_TRACK];
};

/// @param[in] duration millisecond duration
/// @param[out] data ISO8601 duration: P[n]Y[n]M[n]DT[n]H[n]M[n]S
static int ISO8601Duration(int64_t duration, char* data, int size)
{
    int n = 1;
    data[0] = 'P';
    if (duration > 24 * 3600 * 1000)
    {
        n += snprintf(data + n, size - n, "%dD", (int)(duration / (24 * 3600 * 1000)));
        duration %= 24 * 3600 * 1000;
    }

    data[n++] = 'T';
    if (duration > 3600 * 1000)
    {
        n += snprintf(data + n, size - n, "%dH", (int)(duration / (3600 * 1000)));
        duration %= 3600 * 1000;

        n += snprintf(data + n, size - n, "%dM", (int)(duration / (60 * 1000)));
        duration %= 60 * 1000;
    }

    n += snprintf(data + n, size - n, "%dS", (int)((duration + 999) / 1000));
    duration %= 1000;

    return n;
}

static int mov_buffer_read(void* param, void* data, uint64_t bytes)
{
	struct dash_adaptation_set_t* dash;
	dash = (struct dash_adaptation_set_t*)param;
	if (dash->offset + bytes > dash->bytes)
		return E2BIG;
	memcpy(data, dash->ptr + dash->offset, (size_t)bytes);
	return 0;
}

static int mov_buffer_write(void* param, const void* data, uint64_t bytes)
{
	void* ptr;
	size_t capacity;
	struct dash_adaptation_set_t* dash;
	dash = (struct dash_adaptation_set_t*)param;
	if (dash->offset + bytes > dash->maxsize)
		return E2BIG;

	if (dash->offset + (size_t)bytes > dash->capacity)
	{
		capacity = dash->offset + (size_t)bytes + N_SEGMENT;
		capacity = capacity > dash->maxsize ? dash->maxsize : capacity;
		ptr = realloc(dash->ptr, capacity);
		if (NULL == ptr)
			return ENOMEM;
		dash->ptr = ptr;
		dash->capacity = capacity;
	}

	memcpy(dash->ptr + dash->offset, data, (size_t)bytes);
	dash->offset += (size_t)bytes;
	if (dash->offset > dash->bytes)
		dash->bytes = dash->offset;
	return 0;
}

static int mov_buffer_seek(void* param, uint64_t offset)
{
	struct dash_adaptation_set_t* dash;
	dash = (struct dash_adaptation_set_t*)param;
	if (offset >= dash->maxsize)
		return E2BIG;
	dash->offset = (size_t)offset;
	return 0;
}

static uint64_t mov_buffer_tell(void* param)
{
	return ((struct dash_adaptation_set_t*)param)->offset;
}

static struct mov_buffer_t s_io = {
	mov_buffer_read,
	mov_buffer_write,
	mov_buffer_seek,
	mov_buffer_tell,
};

static int dash_adaptation_set_segment(struct dash_mpd_t* mpd, struct dash_adaptation_set_t* track)
{
	int r;
	char name[N_NAME + 32];
	struct list_head *link;
	struct dash_segment_t* seg;

	r = fmp4_writer_save_segment(track->fmp4);
	if (0 != r)
		return r;

	seg = (struct dash_segment_t*)calloc(1, sizeof(*seg));
    if(!seg)
        return -1; // ENOMEM
	seg->timestamp = track->dts;
	seg->duration = track->dts_last - track->dts;

	if(MOV_OBJECT_AAC == track->object)
		snprintf(name, sizeof(name), "%s-%" PRId64 ".m4a", track->prefix, seg->timestamp);
	else
		snprintf(name, sizeof(name), "%s-%" PRId64 ".m4v", track->prefix, seg->timestamp);
	r = mpd->handler(mpd->param, track->setid, track->ptr, track->bytes, track->pts, track->dts, seg->duration, name);
	if (0 != r)
	{
		free(seg);
		return r;
	}

	// link
	list_insert_after(&seg->link, track->root.prev);

	track->count += 1;
	if (DASH_DYNAMIC == mpd->flags && track->count > N_COUNT)
	{
		link = track->root.next;
		list_remove(link);
		seg = list_entry(link, struct dash_segment_t, link);
		free(seg);
		--track->count;
	}
	return 0;
}

static int dash_mpd_flush(struct dash_mpd_t* mpd)
{
	int i, r;
	struct dash_adaptation_set_t* track;

	for (r = i = 0; i < mpd->count && 0 == r; i++)
	{
		track = mpd->tracks + i;
		if (track->raw_bytes)
		{
			r = dash_adaptation_set_segment(mpd, track);
			
			// update maximum segment duration
			mpd->max_segment_duration = MAX(track->dts_last - track->dts, mpd->max_segment_duration);
			if(track->dts_last > track->dts)
				track->bitrate = MAX(track->bitrate, (int)(track->raw_bytes * 1000 / (track->dts_last - track->dts) * 8));	
		}

		track->pts = INT64_MIN;
		track->dts = INT64_MIN;
		track->raw_bytes = 0;

		// reset track buffer
		track->offset = 0;
		track->bytes = 0;
	}

	return r;
}

struct dash_mpd_t* dash_mpd_create(int flags, dash_mpd_segment segment, void* param)
{
	struct dash_mpd_t* mpd;
	mpd = (struct dash_mpd_t*)calloc(1, sizeof(*mpd));
	if (mpd)
	{
		mpd->flags = flags;
		mpd->handler = segment;
		mpd->param = param;
		mpd->time = time(NULL);
	}
	return mpd;
}

void dash_mpd_destroy(struct dash_mpd_t* mpd)
{
	int i;
	struct list_head *p, *n;
	struct dash_segment_t *seg;
	struct dash_adaptation_set_t* track;

	dash_mpd_flush(mpd);

	for (i = 0; i < mpd->count; i++)
	{
		track = &mpd->tracks[i];

		if (track->ptr)
		{
			free(track->ptr);
			track->ptr = NULL;
		}

		list_for_each_safe(p, n, &track->root)
		{
			seg = list_entry(p, struct dash_segment_t, link);
			free(seg);
		}
	}

	free(mpd);
}

int dash_mpd_add_video_adaptation_set(struct dash_mpd_t* mpd, const char* prefix, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size)
{
	int r;
	char name[N_NAME + 16];
	struct dash_adaptation_set_t* track;

	r = (int)strlen(prefix);
	if (mpd->count + 1 >= N_TRACK || extra_data_size < 4 || r >= N_NAME)
		return -1;

	assert(MOV_OBJECT_H264 == object);
	track = &mpd->tracks[mpd->count];
	memcpy(track->prefix, prefix, r);
	LIST_INIT_HEAD(&track->root);
	track->setid = mpd->count++;
	track->object = object;
	track->bitrate = 0;
	track->u.video.width = width;
	track->u.video.height = height;
	track->u.video.frame_rate = 25;
	assert(((const uint8_t*)extra_data)[0] == 1); // configurationVersion
	if (MOV_OBJECT_H264 == object)
	{
		track->u.video.avc.profile = ((const uint8_t*)extra_data)[1];
		track->u.video.avc.compatibility = ((const uint8_t*)extra_data)[2];
		track->u.video.avc.level = ((const uint8_t*)extra_data)[3];
	}

	track->seq = 1;
	track->maxsize = N_FILESIZE;
	track->fmp4 = fmp4_writer_create(&s_io, track, MOV_FLAG_SEGMENT);
	if (!track->fmp4)
		return -1;
	track->track = fmp4_writer_add_video(track->fmp4, object, width, height, extra_data, extra_data_size);
	
	// save init segment file
	r = fmp4_writer_init_segment(track->fmp4);
	if (0 == r)
	{
		snprintf(name, sizeof(name), "%s-init.m4v", prefix);
		r = mpd->handler(mpd->param, mpd->count, track->ptr, track->bytes, 0, 0, 0, name);
	}

	track->bytes = 0;
	track->offset = 0;
	return 0 == r ? track->setid : r;
}

int dash_mpd_add_audio_adaptation_set(struct dash_mpd_t* mpd, const char* prefix, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
{
	int r;
	char name[N_NAME + 16];
	struct dash_adaptation_set_t* track;

	r = (int)strlen(prefix);
	if (mpd->count + 1 >= N_TRACK || extra_data_size < 2 || r >= N_NAME)
		return -1;

	assert(MOV_OBJECT_AAC == object);
	track = &mpd->tracks[mpd->count];
	memcpy(track->prefix, prefix, r);
	LIST_INIT_HEAD(&track->root);
	track->setid = mpd->count++;
	track->object = object;
	track->bitrate = 0;
	track->u.audio.channel = channel_count;
	track->u.audio.sample_bit = bits_per_sample;
	track->u.audio.sample_rate = sample_rate;
	track->u.audio.profile = ((const uint8_t*)extra_data)[0] >> 3;
	if(MOV_OBJECT_AAC == object && 31 == track->u.audio.profile)
		track->u.audio.profile = 32 + (((((const uint8_t*)extra_data)[0] & 0x07) << 3) | ((((const uint8_t*)extra_data)[1] >> 5) & 0x07));

	track->seq = 1;
	track->maxsize = N_FILESIZE;
	track->fmp4 = fmp4_writer_create(&s_io, track, MOV_FLAG_SEGMENT);
	if (!track->fmp4)
		return -1;
	track->track = fmp4_writer_add_audio(track->fmp4, object, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size);

	r = fmp4_writer_init_segment(track->fmp4);
	if (0 == r)
	{
		snprintf(name, sizeof(name), "%s-init.m4a", prefix);
		r = mpd->handler(mpd->param, mpd->count, track->ptr, track->bytes, 0, 0, 0, name);
	}

	track->bytes = 0;
	track->offset = 0;
	return 0 == r ? track->setid : r;
}

int dash_mpd_input(struct dash_mpd_t* mpd, int adapation, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	int r = 0;
	struct dash_adaptation_set_t* track;
	if (adapation >= mpd->count || adapation < 0)
		return -1;

	track = &mpd->tracks[adapation];
	if (NULL == data || 0 == bytes // flash fragment
		|| ((MOV_AV_FLAG_KEYFREAME & flags) && (MOV_OBJECT_H264 == track->object || MOV_OBJECT_HEVC == track->object)))
	{
		r = dash_mpd_flush(mpd);

		// FIXME: live duration
		mpd->duration += mpd->max_segment_duration;
	}

	if (NULL == data || 0 == bytes)
		return r;

	if (0 == track->raw_bytes)
	{
		track->pts = pts;
		track->dts = dts;
	}
	track->dts_last = dts;
	track->raw_bytes += bytes;
	return fmp4_writer_write(track->fmp4, track->track, data, bytes, pts, dts, flags);
}

// ISO/IEC 23009-1:2014(E) 5.4 Media Presentation Description updates (p67)
// 1. the value of MPD@id, if present, shall be the same in the original and the updated MPD;
// 2. the values of any Period@id attributes shall be the same in the original and the updated MPD, unless the containing Period element has been removed;
// 3. the values of any AdaptationSet@id attributes shall be the same in the original and the updated MPD unless the containing Period element has been removed;
size_t dash_mpd_playlist(struct dash_mpd_t* mpd, char* playlist, size_t bytes)
{
	// ISO/IEC 23009-1:2014(E)
	// G.2 Example for ISO Base media file format Live profile (141)
	static const char* s_mpd_dynamic =
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

	static const char* s_h264 =
		"    <AdaptationSet contentType=\"video\" segmentAlignment=\"true\" bitstreamSwitching=\"true\">\n"
		"      <Representation id=\"H264\" mimeType=\"video/mp4\" codecs=\"avc1.%02x%02x%02x\" width=\"%d\" height=\"%d\" frameRate=\"%d\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
		"        <SegmentTemplate timescale=\"1000\" media=\"%s-$Time$.m4v\" initialization=\"%s-init.m4v\">\n"
		"          <SegmentTimeline>\n";

	static const char* s_h265 =
		"    <AdaptationSet contentType=\"video\" segmentAlignment=\"true\" bitstreamSwitching=\"true\">\n"
		"      <Representation id=\"H265\" mimeType=\"video/mp4\" codecs=\"hvc1.%02x%02x%02x\" width=\"%d\" height=\"%d\" frameRate=\"%d\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
		"        <SegmentTemplate timescale=\"1000\" media=\"%s-$Time$.m4v\" initialization=\"%s-init.m4v\">\n"
		"          <SegmentTimeline>\n";

	static const char* s_aac =
		"    <AdaptationSet contentType=\"audio\" segmentAlignment=\"true\" bitstreamSwitching=\"true\">\n"
		"      <Representation id=\"AAC\" mimeType=\"audio/mp4\" codecs=\"mp4a.40.%u\" audioSamplingRate=\"%d\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
		"		 <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"%d\"/>\n"
		"        <SegmentTemplate timescale=\"1000\" media=\"%s-$Time$.m4a\" initialization=\"%s-init.m4a\">\n"
		"          <SegmentTimeline>\n";

	static const char* s_footer =
		"          </SegmentTimeline>\n"
		"        </SegmentTemplate>\n"
		"      </Representation>\n"
		"    </AdaptationSet>\n";

	int i;
	size_t n;
	time_t now;
	char publishTime[32];
	char availabilityStartTime[32];
	unsigned int minimumUpdatePeriod;
	unsigned int timeShiftBufferDepth;
	struct dash_adaptation_set_t* track;
	struct dash_segment_t *seg;
	struct list_head *link;

	now = time(NULL);
	strftime(availabilityStartTime, sizeof(availabilityStartTime), "%Y-%m-%dT%H:%M:%SZ", gmtime(&mpd->time));
	strftime(publishTime, sizeof(publishTime), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
	
	minimumUpdatePeriod = (unsigned int)MAX(mpd->max_segment_duration / 1000, 1);

	if (mpd->flags == DASH_DYNAMIC)
	{
		timeShiftBufferDepth = minimumUpdatePeriod * N_COUNT + 1;
		n = snprintf(playlist, bytes, s_mpd_dynamic, minimumUpdatePeriod, timeShiftBufferDepth, availabilityStartTime, minimumUpdatePeriod, publishTime);
		n += snprintf(playlist + n, bytes - n, "  <Period start=\"PT0S\" id=\"dash\">\n");
	}
	else
	{
		n = snprintf(playlist, bytes, s_mpd_static, (unsigned int)(mpd->duration / 1000), minimumUpdatePeriod);
		n += snprintf(playlist + n, bytes - n, "  <Period start=\"PT0S\" id=\"dash\">\n");
	}

	for (i = 0; i < mpd->count; i++)
	{
		track = &mpd->tracks[i];
		if (MOV_OBJECT_H264 == track->object)
		{
			n += snprintf(playlist + n, bytes - n, s_h264, (unsigned int)track->u.video.avc.profile, (unsigned int)track->u.video.avc.compatibility, (unsigned int)track->u.video.avc.level, track->u.video.width, track->u.video.height, track->u.video.frame_rate, track->bitrate, track->prefix, track->prefix);
			list_for_each(link, &track->root)
			{
				seg = list_entry(link, struct dash_segment_t, link);
				n += snprintf(playlist + n, bytes - n, "             <S t=\"%" PRId64 "\" d=\"%u\"/>\n", seg->timestamp, (unsigned int)seg->duration);
			}
			n += snprintf(playlist + n, bytes - n, "%s", s_footer);
		}
		else if (MOV_OBJECT_HEVC == track->object)
		{
			n += snprintf(playlist + n, bytes - n, s_h265, (unsigned int)track->u.video.avc.profile, (unsigned int)track->u.video.avc.compatibility, (unsigned int)track->u.video.avc.level, track->u.video.width, track->u.video.height, track->u.video.frame_rate, track->bitrate, track->prefix, track->prefix);
			list_for_each(link, &track->root)
			{
				seg = list_entry(link, struct dash_segment_t, link);
				n += snprintf(playlist + n, bytes - n, "             <S t=\"%" PRId64 "\" d=\"%u\"/>\n", seg->timestamp, (unsigned int)seg->duration);
			}
			n += snprintf(playlist + n, bytes - n, "%s", s_footer);
		}
		else if (MOV_OBJECT_AAC == track->object)
		{
			n += snprintf(playlist + n, bytes - n, s_aac, (unsigned int)track->u.audio.profile, track->u.audio.sample_rate, track->bitrate, track->u.audio.channel, track->prefix, track->prefix);
			list_for_each(link, &track->root)
			{
				seg = list_entry(link, struct dash_segment_t, link);
				n += snprintf(playlist + n, bytes - n, "             <S t=\"%" PRId64 "\" d=\"%u\"/>\n", seg->timestamp, (unsigned int)seg->duration);
			}
			n += snprintf(playlist + n, bytes - n, "%s", s_footer);
		}
		else
		{
			assert(0);
		}
	}

	n += snprintf(playlist + n, bytes - n, "  </Period>\n</MPD>\n");
	return n;
}
