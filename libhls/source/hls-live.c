#include "hls-live.h"
#include "hls-file.h"
#include "hls-server.h"
#include "h264-util.h"
#include <stdlib.h>
#include <memory.h>

static struct list_head s_head;

static void hls_live_onwrite(void* param, const void* packet, int bytes)
{
	struct hls_live_t *live;
	live = (struct hls_live_t *)param;

	// write file
	if(live->file)
	{
		hls_file_write(live->file, packet, bytes);
	}
}

static struct hls_live_t* hls_live_create(struct hls_server_t* server, const char* name)
{
	struct hls_live_t* live;

	live = (struct hls_live_t*)malloc(sizeof(live[0]));
	if(!live)
		return NULL;

	memset(live, 0, sizeof(live[0]));
	locker_create(&live->locker);
	strcpy(live->name, name);
	live->pts = 412536249;
	live->refcnt = 1;
	live->m3u8seq = 0;
	live->server = server;
	live->ts = mpeg_ts_create(hls_live_onwrite, live);

	// init header link
	if(!s_head.next)
	{
		s_head.next = &s_head;
		s_head.prev = &s_head;
	}
	list_insert_after(&live->link, &s_head); // link
	return live;
}

static int hls_live_destroy(struct hls_live_t* live)
{
	if(live->ts)
		mpeg_ts_destroy(live->ts);

	locker_destroy(&live->locker);

	list_remove(&live->link); // unlink

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

struct hls_live_t* hls_live_fetch(struct hls_server_t* ctx, const char* name)
{
	struct hls_live_t *live;

	// init header link
	if(!s_head.next)
	{
		s_head.next = &s_head;
		s_head.prev = &s_head;
	}

	live = hls_live_find(name);
	if(live)
		return live;

	return hls_live_create(ctx, name);
}

int hls_live_release(struct hls_live_t* live)
{
	if(0 == InterlockedDecrement(&live->refcnt))
	{
		hls_live_destroy(live);
	}

	return 0;
}

int hls_live_m3u8(struct hls_live_t* live, char* m3u8)
{
	int i;
	char msg[64];
	struct hls_file_t *file;

	locker_lock(&live->locker);

	sprintf(m3u8, 
		"#EXTM3U\n" // MUST
		"#EXT-X-VERSION:3\n" // Optional
		"#EXT-X-TARGETDURATION:%d\n" // MUST
		"#EXT-X-MEDIA-SEQUENCE:%d\n" // Live
		"#EXT-X-ALLOW-CACHE:NO\n",
		MAX_DURATION, live->m3u8seq);

	for(i = 0; i < live->file_count; i++)
	{
		file = live->files[i];
		sprintf(msg, "#EXTINF:%d,live\n%s/%d.ts\n", file->duration/1000, live->name, file->seq);
		strcat(m3u8, msg);
	}

	live->rtime = time64_now();
	locker_unlock(&live->locker);
	return 0;
}

struct hls_file_t* hls_live_read(struct hls_live_t* live, char* name)
{
	int i, seq;
	struct hls_file_t* file;

	seq = atoi(name);

	locker_lock(&live->locker);
	live->rtime = time64_now();

	for(i = 0; i < live->file_count; i++)
	{
		file = live->files[i];
		if(file->seq == seq)
		{
			InterlockedIncrement(&file->refcnt);
			locker_unlock(&live->locker);
			return file;
		}
	}

	locker_unlock(&live->locker);
	return NULL;
}

int hls_live_input(struct hls_live_t* live, const void* data, int bytes, int stream)
{
	live->wtime = time64_now(); // last write time

	if(STREAM_VIDEO_H264==stream && h264_idr(data, bytes))
	{
		// update m3u8 file list
		if(live->file)
		{
			live->file->duration = time64_now() - live->file->tcreate;
			live->file->seq = live->m3u8seq;
			locker_lock(&live->locker);
			assert(live->file_count <= MAX_FILES);
			if(live->file_count >= MAX_FILES)
			{
				// remove oldest file
				hls_file_close(live->files[0]); // TODO: do it out of locker
				memmove(live->files, live->files+1, (MAX_FILES-1)*sizeof(live->files[0]));
				live->file_count = MAX_FILES - 1;
			}
			live->files[live->file_count++] = live->file;
			live->m3u8seq = (live->m3u8seq + 1) % 0x7fffffff; // update EXT-X-MEDIA-SEQUENCE
			locker_unlock(&live->locker);

			// reset mpeg ts generator
			mpeg_ts_reset(live->ts);
		}

		// create new file
		live->file = hls_file_open();
	}

	if(STREAM_VIDEO_H264 == stream)
	{
		live->pts += 90 * 40; // 90kHZ * 40ms
	}
	else
	{
		assert(STREAM_AUDIO_AAC == stream);
		live->pts += 90 * 40; // 90kHZ * 40ms
	}

	return mpeg_ts_write(live->ts, stream, live->pts, live->pts, data, bytes);
}
