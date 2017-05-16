#include "hls-m3u8.h"
#include "hls-param.h"
#include "list.h"
#include <inttypes.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define VMAX(a, b) ((a) > (b) ? (a) : (b))

struct hls_m3u8_t
{
	int live;
	int64_t seq; // m3u8 sequence number (base 0)
	int64_t duration;// target duration

	size_t count;
	struct list_head root;
};

struct hls_segment_t
{
	struct list_head link;

	int64_t pts;		// present timestamp (millisecond)
	int64_t duration;	// segment duration (millisecond)
	int discontinuity;	// EXT-X-DISCONTINUITY flag

	char name[128];
};

void* hls_m3u8_create(int live)
{
	struct hls_m3u8_t* m3u8;
	m3u8 = (struct hls_m3u8_t*)malloc(sizeof(*m3u8));
	if (NULL == m3u8)
		return NULL;

	assert(0 == live || live >= HLS_LIVE_NUM);
	m3u8->live = live;
	m3u8->seq = 0;
	m3u8->count = 0;
	m3u8->duration = 0;
	LIST_INIT_HEAD(&m3u8->root);
	return m3u8;
}

void hls_m3u8_destroy(void* p)
{
	struct hls_m3u8_t* m3u8;
	struct list_head* l, *n;
	struct hls_segment_t* seg;
	m3u8 = (struct hls_m3u8_t*)p;

	list_for_each_safe(l, n, &m3u8->root)
	{
		seg = list_entry(l, struct hls_segment_t, link);
		free(seg);
	}

	free(m3u8);
}

int hls_m3u8_add(void* p, const char* name, int64_t pts, int64_t duration, int discontinuity)
{
	int r;
	struct hls_m3u8_t* m3u8;
	struct hls_segment_t* seg;
	m3u8 = (struct hls_m3u8_t*)p;
	
	if (0 == m3u8->live || m3u8->count < (size_t)m3u8->live)
	{
		seg = (struct hls_segment_t*)malloc(sizeof(*seg));
		if (!seg)
			return ENOMEM;
		++m3u8->count;
	}
	else
	{
		++m3u8->seq; // update EXT-X-MEDIA-SEQUENCE

		// reuse the first segment
		seg = list_entry(m3u8->root.next, struct hls_segment_t, link);
		list_remove(&seg->link);
	}

	// update EXT-X-TARGETDURATION
	m3u8->duration = VMAX(m3u8->duration, duration);

	// segment
	seg->pts = pts;
	seg->duration = duration;
	seg->discontinuity = discontinuity; // EXT-X-DISCONTINUITY
	r = snprintf(seg->name, sizeof(seg->name), "%s", name);
	if (r <= 0 || r >= sizeof(seg->name))
	{
		free(seg);
		return ENOMEM;
	}

	list_insert_after(&seg->link, m3u8->root.prev);
	return 0;
}

size_t hls_m3u8_count(void* m3u8)
{
	return ((struct hls_m3u8_t*)m3u8)->count;
}

int hls_m3u8_playlist(void* p, int eof, char* playlist, size_t bytes)
{
	int r;
	size_t n;
	struct list_head* link;
	struct hls_m3u8_t* m3u8;
	struct hls_segment_t* seg;
	m3u8 = (struct hls_m3u8_t*)p;

	r = snprintf(playlist, bytes,
		"#EXTM3U\n" // MUST
		"#EXT-X-VERSION:3\n" // Optional
		"#EXT-X-TARGETDURATION:%" PRId64 "\n" // MUST, decimal-integer, in seconds
		"#EXT-X-MEDIA-SEQUENCE:%" PRId64 "\n"
		"%s"  // #EXT-X-PLAYLIST-TYPE:VOD
		"%s", // #EXT-X-ALLOW-CACHE:YES
		(m3u8->duration + 999) / 1000,
		m3u8->seq,
		m3u8->live ? "" : "#EXT-X-PLAYLIST-TYPE:VOD\n",
		m3u8->live ? "" : "#EXT-X-ALLOW-CACHE:YES\n");
	if (r <= 0 || (size_t)r >= bytes)
		return ENOMEM;

	n = r;
	list_for_each(link, &m3u8->root)
	{
		if (bytes <= n)
			break;

		seg = list_entry(link, struct hls_segment_t, link);

		if (seg->discontinuity)
			n += snprintf(playlist + n, bytes - n, "#EXT-X-DISCONTINUITY\n");
		if(bytes > n)
			n += snprintf(playlist + n, bytes - n, "#EXTINF:%.3f\n%s\n", seg->duration / 1000.0, seg->name);
	}

	if (eof && bytes > n + 15)
		n += snprintf(playlist + n, bytes - n, "#EXT-X-ENDLIST\n");

	return (bytes > n && n > 0) ? 0 : ENOMEM;
}
