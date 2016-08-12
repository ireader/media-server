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
	struct hls_segment_t* segment;

	void* ts;
    uint8_t* ptr;
	uint32_t bytes;
	uint32_t capacity;

	uint64_t vpacket;		// video packet in segment
	int64_t duration;		// maximum segment duration
	int64_t target_duration;// maximum segment duration
	int64_t pts;			// last packet pts

	hls_vod_handler handler;
	void* param;
};

struct hls_segments_t
{
	struct list_head link;
	struct hls_segment_t segments[50];
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
	hls = (struct hls_vod_t*)p;

	locker_destroy(&hls->locker);

	if (hls->ts)
		mpeg_ts_destroy(hls->ts);

	if (hls->ptr)
	{
		assert(hls->capacity > 0);
		free(hls->ptr);
	}

#if defined(_DEBUG) || defined(DEBUG)
	memset(hls, 0xCC, sizeof(*hls));
#endif
	free(hls);
}

static int hls_segment_fetch(struct hls_vod_t* hls)
{
	struct hls_segments_t* s;

	locker_lock(&hls->locker);
	s = list_entry(hls->root.prev, struct hls_segments_t, link);
	locker_unlock(&hls->locker);

	if (list_empty(&hls->root) || s->num + 1 >= N_SEGMENT)
	{
		s = (struct hls_segments_t*)malloc(sizeof(*s));
		if (NULL == s)
			return ENOMEM;
		memset(s, 0, sizeof(*s));

		locker_lock(&hls->locker);
		list_insert_after(&s->link, hls->root.prev);
		s = list_entry(hls->root.prev, struct hls_segments_t, link);
		locker_unlock(&hls->locker);
	}
	
	assert(s->num + 1 < N_SEGMENT);
	hls->segment = s->segments + s->num++;
	return 0;
}

int hls_vod_input(void* p, int avtype, const void* data, unsigned int bytes, int64_t pts, int64_t dts)
{
	int r;
	struct hls_vod_t* hls;
	hls = (struct hls_vod_t*)p;

	if (NULL == hls->segment || 
		(pts - hls->segment->pts > hls->duration &&  // duration check
			(0 == hls->vpacket || (STREAM_VIDEO_H264 == avtype && h264_idr((const uint8_t*)data, bytes)))))  // IDR-frame or audio only stream
	{
		if (NULL != hls->segment)
		{
			// fill file information
			hls->segment->duration = hls->pts - hls->segment->pts;
			hls->target_duration = hls->target_duration > hls->segment->duration ? hls->target_duration : hls->segment->duration; // update EXT-X-TARGETDURATION
			
			// new file callback
			hls->handler(hls->param, hls->ptr, hls->bytes, hls->segment->pts, hls->segment->duration, hls->segment->m3u8seq, hls->segment->name);

			// reset mpeg ts generator
			mpeg_ts_reset(hls->ts);
			hls->segment = NULL;
		}

		// new segment
		r = hls_segment_fetch(hls);
		if (0 != r || NULL == hls->segment)
			return r;

		hls->segment->pts = pts;
		hls->segment->m3u8seq = hls->m3u8seq++; // EXT-X-MEDIA-SEQUENCE
		hls->segment->discontinue = EXT_X_DISCONTINUITY(pts, hls->pts); // EXT-X-DISCONTINUITY
		hls->vpacket = 0;
		hls->bytes = 0;
	}

	if (STREAM_VIDEO_H264 == avtype)
		hls->vpacket++;

	hls->pts = pts;
	return mpeg_ts_write(hls->ts, avtype, pts * 90, dts * 90, data, bytes);
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
			n += snprintf(m3u8 + n, bytes - n, "#EXTINF:%.3f\n%s\n", segment->duration / 1000.0, segment->name);
		}
	}

	if(bytes - n > 10)
		n = snprintf(m3u8 + n, bytes-n, "#EXT-X-ENDLIST\n");

	return n;
}
