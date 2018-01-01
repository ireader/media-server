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
	int version;
	int64_t seq; // m3u8 sequence number (base 0)
	int64_t duration;// target duration

	size_t count;
	struct list_head root;

	char* ext_x_map;
};

struct hls_segment_t
{
	struct list_head link;

	int64_t pts;		// present timestamp (millisecond)
	int64_t duration;	// segment duration (millisecond)
	int discontinuity;	// EXT-X-DISCONTINUITY flag

	char* name;
	size_t capacity;
};

struct hls_m3u8_t* hls_m3u8_create(int live, int version)
{
	struct hls_m3u8_t* m3u8;
	m3u8 = (struct hls_m3u8_t*)calloc(1, sizeof(*m3u8));
	if (NULL == m3u8)
		return NULL;

	assert(0 == live || live >= HLS_LIVE_NUM);
	m3u8->version = version;
	m3u8->live = live;
	m3u8->seq = 0;
	m3u8->count = 0;
	m3u8->duration = 0;
	LIST_INIT_HEAD(&m3u8->root);
	return m3u8;
}

void hls_m3u8_destroy(struct hls_m3u8_t* m3u8)
{
	struct list_head* l, *n;
	struct hls_segment_t* seg;
	list_for_each_safe(l, n, &m3u8->root)
	{
		seg = list_entry(l, struct hls_segment_t, link);
		free(seg);
	}

	if (m3u8->ext_x_map)
		free(m3u8->ext_x_map);
	free(m3u8);
}

static struct hls_segment_t* hls_segment_alloc(size_t bytes)
{
	struct hls_segment_t* seg;
	seg = (struct hls_segment_t*)malloc(sizeof(*seg) + bytes);
	if (seg)
	{
		seg->name = (char*)(seg + 1);
		seg->capacity = bytes;
	}
	return seg;
}

int hls_m3u8_add(struct hls_m3u8_t* m3u8, const char* name, int64_t pts, int64_t duration, int discontinuity)
{
	size_t r;
	struct hls_segment_t* seg;
	seg = NULL;
	r = strlen(name);
	
	if(0 != m3u8->live && m3u8->count >= (size_t)m3u8->live)
	{
		assert(m3u8->count == (size_t)m3u8->live);

		++m3u8->seq; // update EXT-X-MEDIA-SEQUENCE

		// reuse the first segment
		seg = list_entry(m3u8->root.next, struct hls_segment_t, link);
		list_remove(&seg->link);

		// check name length
		if (r + 1 > seg->capacity)
		{
			free(seg);
			seg = NULL;	
			--m3u8->count;
		}
	}

	if (NULL == seg)
	{
		// reserve more space for reuse segment
		seg = hls_segment_alloc(r + (m3u8->live ? 16 : 1));
		if (!seg)
			return ENOMEM;

		++m3u8->count;
	}

	// update EXT-X-TARGETDURATION
	m3u8->duration = VMAX(m3u8->duration, duration);

	// segment
	seg->pts = pts;
	seg->duration = duration;
	seg->discontinuity = discontinuity; // EXT-X-DISCONTINUITY
	memcpy(seg->name, name, r + 1); // copy last '\0'

	list_insert_after(&seg->link, m3u8->root.prev);
	return 0;
}

int hls_m3u8_set_x_map(hls_m3u8_t* m3u8, const char* name)
{
	if (m3u8->ext_x_map)
		free(m3u8->ext_x_map);
	m3u8->ext_x_map = name ? strdup(name) : NULL;
	return m3u8->ext_x_map ? 0 : ENOMEM;
}

size_t hls_m3u8_count(struct hls_m3u8_t* m3u8)
{
	return m3u8->count;
}

int hls_m3u8_playlist(struct hls_m3u8_t* m3u8, int eof, char* playlist, size_t bytes)
{
	int r;
	size_t n;
	struct list_head* link;
	struct hls_segment_t* seg;

	r = snprintf(playlist, bytes,
		"#EXTM3U\n" // MUST
		"#EXT-X-VERSION:%d\n" // Optional
		"#EXT-X-TARGETDURATION:%" PRId64 "\n" // MUST, decimal-integer, in seconds
		"#EXT-X-MEDIA-SEQUENCE:%" PRId64 "\n"
		"%s"  // #EXT-X-PLAYLIST-TYPE:VOD
		"%s", // #EXT-X-ALLOW-CACHE:YES
		m3u8->version,
		(m3u8->duration + 999) / 1000,
		m3u8->seq,
		m3u8->live ? "" : "#EXT-X-PLAYLIST-TYPE:VOD\n",
		m3u8->live ? "" : "#EXT-X-ALLOW-CACHE:YES\n");
	if (r <= 0 || (size_t)r >= bytes)
		return ENOMEM;

	// #EXT-X-MAP:URI="main.mp4",BYTERANGE="1206@0"
	if (m3u8->ext_x_map)
		r += snprintf(playlist + r, bytes - r, "#EXT-X-MAP:URI=\"%s\",\n", m3u8->ext_x_map);

	n = r;
	list_for_each(link, &m3u8->root)
	{
		if (bytes <= n)
			break;

		seg = list_entry(link, struct hls_segment_t, link);

		if (seg->discontinuity)
			n += snprintf(playlist + n, bytes - n, "#EXT-X-DISCONTINUITY\n");
		if(bytes > n)
			n += snprintf(playlist + n, bytes - n, "#EXTINF:%.3f,\n%s\n", seg->duration / 1000.0, seg->name);
	}

	if (eof && bytes > n + 15)
		n += snprintf(playlist + n, bytes - n, "#EXT-X-ENDLIST\n");

	return (bytes > n && n > 0) ? 0 : ENOMEM;
}
