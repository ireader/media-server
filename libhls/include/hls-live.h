#ifndef _hls_live_h_
#define _hls_live_h_

#include "cstringext.h"
#include "sys/sync.h"
#include "list.h"
#include "time64.h"
#include "mpeg-ts.h"

#define MAX_NAME 64
#define MAX_FILES 25
#define MAX_DURATION 5 // 10s, from Apple recommendation

struct hls_file_t;
struct hls_server_t;

struct hls_live_t
{
	struct list_head link;
	struct hls_server_t* server;

	long refcnt;
	locker_t locker;
	char name[MAX_NAME];

	unsigned int m3u8seq; // EXT-X-MEDIA-SEQUENCE

	struct hls_file_t *file; // temporary file
	struct hls_file_t *files[MAX_FILES];
	int file_count;

	void* ts;
	int64_t pts;
	time64_t rtime; // last read time
	time64_t wtime; // last write time
};

struct hls_live_t* hls_live_fetch(struct hls_server_t* ctx, const char* name);
int hls_live_release(struct hls_live_t* live);

int hls_live_m3u8(struct hls_live_t* live, char* m3u8);

int hls_live_input(struct hls_live_t* live, const void* data, int bytes, int stream);

struct hls_file_t* hls_live_read(struct hls_live_t* live, char* file);

#endif /* !_hls_live_h_ */
