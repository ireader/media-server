#include "hls-vod.h"
#include "hls-param.h"
#include "hls-segment.h"
#include "mpeg-ts.h"
#include "mpeg-ps.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "list.h"
#include "h264-util.h"
#include "ctypedef.h"
#include <stdio.h>

#define N_SEGMENT	50
#define N_TS_PACKET 188

#define EXT_X_DISCONTINUITY(x, y) ( ((x) < (y) || (x) - (y) > 30000) ? 1 : 0 )

struct hls_vod_t
{
	unsigned int m3u8seq; // start from 0

	locker_t locker;
	struct list_head root;

	void* ts;
    uint8_t* ptr;
	uint32_t bytes;
	uint32_t capacity;

	uint64_t vpacket;		// video packet in segment
	int64_t duration;		// user setting segment duration
	int64_t target_duration;// maximum segment duration
	int64_t pts;			// last packet pts
	int64_t segment_pts;

	hls_vod_handler handler;
	void* param;
};

struct hls_segments_t
{
	struct list_head link;
	struct hls_segment_t segments[N_SEGMENT];
	int num;
};

static void* hls_ts_alloc(void* param, size_t bytes)
{
	struct hls_vod_t* hls;
	hls = (struct hls_vod_t*)param;
	assert(188 == bytes);
	assert(hls->capacity >= hls->bytes);
	if (hls->capacity - hls->bytes < bytes)
	{
		void* p = realloc(hls->ptr, hls->capacity + bytes + N_TS_PACKET * 10 * 1024);
		if (NULL == p)
			return NULL;
		hls->ptr = p;
		hls->capacity += bytes + N_TS_PACKET * 10 * 1024;
	}
	return hls->ptr + hls->bytes;
}

static void hls_ts_free(void* param, void* packet)
{
	struct hls_vod_t* hls;
	hls = (struct hls_vod_t*)param;
	assert(packet == hls->ptr + hls->bytes - 188);
	assert(hls->ptr <= (uint8_t*)packet && hls->ptr + hls->capacity > (uint8_t*)packet);
}

static void hls_ts_write(void* param, const void* packet, size_t bytes)
{
	struct hls_vod_t* hls;
	hls = (struct hls_vod_t*)param;
	assert(188 == bytes);
	assert(hls->ptr <= (uint8_t*)packet && hls->ptr + hls->capacity > (uint8_t*)packet);
	hls->bytes += bytes; // update packet length
}

static void* hls_ts_create(struct hls_vod_t* hls)
{
	struct mpeg_ts_func_t handler;
	handler.alloc = hls_ts_alloc;
	handler.write = hls_ts_write;
	handler.free = hls_ts_free;
	return mpeg_ts_create(&handler, hls);
}

void* hls_vod_create(unsigned int duration, hls_vod_handler handler, void* param)
{
	struct hls_vod_t* hls;
	hls = (struct hls_vod_t*)malloc(sizeof(*hls));
	if (NULL == hls)
		return NULL;

	memset(hls, 0, sizeof(struct hls_vod_t));
	hls->ts = hls_ts_create(hls);
	if (NULL == hls->ts)
	{
		free(hls);
		return NULL;
	}

	LIST_INIT_HEAD(&hls->root);
	locker_create(&hls->locker);
	hls->duration = (0 == duration) ? HLS_MIN_DURATION : duration;
	hls->handler = handler;
	hls->param = param;
	return hls;
}

void hls_vod_destroy(void* p)
{
	struct hls_vod_t* hls;
	struct list_head* link, *n;
	struct hls_segments_t* seg;
	hls = (struct hls_vod_t*)p;

	locker_destroy(&hls->locker);

	if (hls->ts)
		mpeg_ts_destroy(hls->ts);

	if (hls->ptr)
	{
		assert(hls->capacity > 0);
		free(hls->ptr);
	}

	list_for_each_safe(link, n, &hls->root)
	{
		seg = list_entry(link, struct hls_segments_t, link);
		free(seg);
	}

#if defined(_DEBUG) || defined(DEBUG)
	memset(hls, 0xCC, sizeof(*hls));
#endif
	free(hls);
}

static struct hls_segment_t* hls_segment_fetch(struct hls_vod_t* hls)
{
	struct hls_segments_t* s;

	locker_lock(&hls->locker);
	s = list_entry(hls->root.prev, struct hls_segments_t, link);
	locker_unlock(&hls->locker);

	if (list_empty(&hls->root) || s->num >= N_SEGMENT)
	{
		s = (struct hls_segments_t*)malloc(sizeof(*s));
		if (NULL == s)
			return NULL;
		memset(s, 0, sizeof(*s));

		locker_lock(&hls->locker);
		list_insert_after(&s->link, hls->root.prev);
		s = list_entry(hls->root.prev, struct hls_segments_t, link);
		locker_unlock(&hls->locker);
	}
	
	assert(s->num < N_SEGMENT);
	return s->segments + s->num++;
}

int hls_vod_input(void* p, int avtype, const void* data, unsigned int bytes, int64_t pts, int64_t dts)
{
	int r;
	struct hls_vod_t* hls;
	struct hls_segment_t* seg;
	hls = (struct hls_vod_t*)p;

	if (0 == hls->bytes ||
		(pts - hls->segment_pts > hls->duration &&  // duration check
			// TODO: check sps/pps???
			(0 == hls->vpacket || (STREAM_VIDEO_H264 == avtype && h264_idr((const uint8_t*)data, bytes)))))  // IDR-frame or audio only stream
	{
		if (hls->bytes > 0)
		{
			// new segment
			seg = hls_segment_fetch(hls);
			if (0 == seg)
				return ENOMEM;

			// fill file information
			seg->pts = hls->segment_pts;
			seg->m3u8seq = hls->m3u8seq++; // EXT-X-MEDIA-SEQUENCE
			seg->duration = hls->pts - hls->segment_pts;
			seg->discontinue = EXT_X_DISCONTINUITY(pts, hls->pts); // EXT-X-DISCONTINUITY
			hls->target_duration = hls->target_duration > seg->duration ? hls->target_duration : seg->duration; // update EXT-X-TARGETDURATION
			
			// new file callback
			r = hls->handler(hls->param, hls->ptr, hls->bytes, seg->pts, seg->duration, seg->m3u8seq, seg->name);
			if (0 != r)
				return r;

			// reset mpeg ts generator
			mpeg_ts_reset(hls->ts);
		}

		hls->segment_pts = pts;
		hls->vpacket = 0;
		hls->bytes = 0;
	}

	if (STREAM_VIDEO_H264 == avtype)
		hls->vpacket++;

	hls->pts = pts;
	return mpeg_ts_write(hls->ts, avtype, pts * 90, dts * 90, data, bytes);
}

int hls_vod_count(void* p)
{
	int n;
	struct hls_vod_t* hls;
	struct list_head* link;
	struct hls_segments_t* seg;
	hls = (struct hls_vod_t*)p;

	n = 0;
	list_for_each(link, &hls->root)
	{
		seg = list_entry(link, struct hls_segments_t, link);
		n += seg->num;
	}

	return n;
}

int hls_vod_m3u8(void* p, char* m3u8, int bytes)
{
	int i, n;
	struct hls_vod_t* hls;
	struct list_head* link;
	hls = (struct hls_vod_t*)p;
	
	n = snprintf(m3u8, bytes,
		"#EXTM3U\n" // MUST
		"#EXT-X-VERSION:3\n" // Optional
		"#EXT-X-TARGETDURATION:%I64d\n" // MUST, decimal-integer, in seconds
//		"EXT-X-PLAYLIST-TYPE:VOD\n"
//		"#EXT-X-MEDIA-SEQUENCE:0\n", // VOD
//		"#EXT-X-ALLOW-CACHE:NO\n"
		, (hls->target_duration+999)/1000);

	list_for_each(link, &hls->root)
	{
		struct hls_segments_t* segments;
		segments = list_entry(link, struct hls_segments_t, link);

		for (i = 0; i < segments->num && bytes - n > 0; i++)
		{
			struct hls_segment_t* segment;
			segment = segments->segments + i;
			if (segment->discontinue)
				n += snprintf(m3u8 + n, bytes - n, "#EXT-X-DISCONTINUITY\n");
			n += snprintf(m3u8 + n, bytes - n, "#EXTINF:%.3f\n%s\n", segment->duration / 1000.0, segment->name);
		}
	}

	if(bytes - n > 10)
		n += snprintf(m3u8 + n, bytes-n, "#EXT-X-ENDLIST\n");

	return n;
}
