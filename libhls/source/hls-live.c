#include "hls-live.h"
#include "hls-file.h"
#include "hls-h264.h"
#include "hls-server.h"
#include "mpeg-ts.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#define VMAX(a, b)	((a) > (b) ? (a) : (b))

static locker_t s_locker; // lock live list(consider lock-free list???)
static struct list_head s_head;

static void* hls_live_alloc(void* param, size_t bytes)
{
	struct hls_live_t *live;
	live = (struct hls_live_t *)param;
	assert(bytes < sizeof(live->tspacket));
	return live->tspacket;
}

static void hls_live_free(void* param, void* packet)
{
	struct hls_live_t *live;
	live = (struct hls_live_t *)param;
	assert(packet == live->tspacket);
}

static void hls_live_write(void* param, const void* packet, size_t bytes)
{
	struct hls_live_t *live;
	live = (struct hls_live_t *)param;

	// write file
	if(live->file)
	{
		hls_file_write(live->file, packet, bytes);
	}
}

static struct hls_live_t* hls_live_create(const char* name, int64_t duration)
{
	struct hls_live_t* live;
	struct mpeg_ts_func_t func;

	live = (struct hls_live_t*)malloc(sizeof(live[0]) + 2*1024*1024);
	if(!live)
		return NULL;

	func.alloc = hls_live_alloc;
	func.free = hls_live_free;
	func.write = hls_live_write;
	memset(live, 0, sizeof(live[0]));
	live->ts = mpeg_ts_create(&func, live);
    if(!live->ts)
    {
        free(live);
        return NULL;
    }

    strncpy(live->name, name, MAX_NAME-1);
    live->pts = 900000;
	live->duration = (0 == duration) ? HLS_DURATION * 1000 : duration;
    live->refcnt = 2; // one for global list
    live->m3u8seq = 0;
    live->vbuffer = (unsigned char*)(live + 1);
    locker_create(&live->locker);

    locker_lock(&s_locker);
	list_insert_after(&live->link, &s_head); // link
    locker_unlock(&s_locker);

	return live;
}

static int hls_live_destroy(struct hls_live_t* live)
{
    locker_lock(&s_locker);
    list_remove(&live->link); // unlink
    locker_unlock(&s_locker);

	if(live->ts)
		mpeg_ts_destroy(live->ts);

	locker_destroy(&live->locker);

	free(live);
	return 0;
}

static struct hls_live_t* hls_live_find(const char* name)
{
	struct list_head *pos;
	struct hls_live_t *live;
	list_for_each(pos, &s_head)
	{
		live = list_entry(pos, struct hls_live_t, link);
		if(0 == strcmp(live->name, name))
			return live;
	}

	return NULL;
}

int hls_live_init(void)
{
    locker_create(&s_locker);
    LIST_INIT_HEAD(&s_head);
    return 0;
}

int hls_live_cleanup(void)
{
    struct hls_live_t *live;
    struct list_head *pos, *next;
    
    list_for_each_safe(pos, next, &s_head)
    {
        live = list_entry(pos, struct hls_live_t, link);
        hls_live_release(live);
    }
    
    locker_destroy(&s_locker);
	return 0;
}

struct hls_live_t* hls_live_fetch(const char* name)
{
	struct hls_live_t *live;

    locker_lock(&s_locker);
	live = hls_live_find(name);
    locker_unlock(&s_locker);
    if(live)
    {
        atomic_increment32(&live->refcnt);
        return live;
    }

	return hls_live_create(name, 2000);
}

int hls_live_release(struct hls_live_t* live)
{
	if(0 == atomic_decrement32(&live->refcnt))
	{
		hls_live_destroy(live);
	}

	return 0;
}

int hls_live_m3u8(struct hls_live_t* live, char* m3u8)
{
	int i, n, t;
	struct hls_file_t *file;

	locker_lock(&live->locker);

	n = sprintf(m3u8,
		"#EXTM3U\n" // MUST
		"#EXT-X-VERSION:3\n" // Optional
		"#EXT-X-TARGETDURATION:%d\n" // MUST
		"#EXT-X-MEDIA-SEQUENCE:%u\n", // Live
//		"#EXT-X-ALLOW-CACHE:NO\n",
		HLS_DURATION,
		VMAX(0, live->m3u8seq - live->file_count));

	for(i = 0; i < (int)live->file_count; i++)
	{
        file = live->files[i];
        t = file->duration / 1000;
        n += sprintf(m3u8 + n, "#EXTINF:%d,live\n%s/%d.ts\n", VMAX(1, t), live->name, file->seq);
	}

    live->rtime = time64_now();
	locker_unlock(&live->locker);
	return n;
}

struct hls_file_t* hls_live_file(struct hls_live_t* live, char* name)
{
	int i, seq;
	struct hls_file_t* file;

	seq = atoi(name);

	locker_lock(&live->locker);
	live->rtime = time64_now();

	for(i = 0; i < (int)live->file_count; i++)
	{
		file = live->files[i];
		if(file->seq == seq)
		{
            locker_unlock(&live->locker);
			atomic_increment32(&file->refcnt);
			return file;
		}
	}

	locker_unlock(&live->locker);
	return NULL;
}

int hls_live_input(struct hls_live_t* live, const void* data, size_t bytes, int stream)
{
    int duration;
    struct hls_file_t *file;

	live->wtime = time64_now(); // last write time

    file = live->file;
    duration = file ? (int)(live->wtime - file->tcreate) : 0;
    //duration = file ? (int)(live->pts - live->file->pts) : 0;

	if( (!file || duration >= live->duration) && HLS_VIDEO_H264==stream && h264_idr(data, bytes) )
	{
		// update m3u8 file list
		if(file)
		{
            // fill file information
            file->duration = duration;
			file->seq = live->m3u8seq;

            // update live file list
            locker_lock(&live->locker);
			assert(live->file_count <= HLS_FILE_NUM);
			if(live->file_count >= HLS_FILE_NUM)
			{
				// remove oldest file
				hls_file_close(live->files[0]); // TODO: do it out of locker
				memmove(live->files, live->files+1, (HLS_FILE_NUM-1)*sizeof(live->files[0]));
				live->file_count = HLS_FILE_NUM - 1;
			}
			live->files[live->file_count++] = file;
			++live->m3u8seq; // update EXT-X-MEDIA-SEQUENCE
			locker_unlock(&live->locker);

			// reset mpeg ts generator
			mpeg_ts_reset(live->ts);
		}

		// create new file
		live->file = hls_file_open();
        live->file->pts = live->pts;
	}

	if(HLS_VIDEO_H264 == stream)
	{
		live->pts += 90 * 40; // 90kHZ * 40ms
	}
	else
	{
		assert(HLS_AUDIO_AAC == stream);
		live->pts += 90 * 40; // 90kHZ * 40ms
	}

	return mpeg_ts_write(live->ts, stream, live->pts, live->pts, data, bytes);
}
