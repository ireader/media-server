#ifndef _hls_live_h_
#define _hls_live_h_

#include "cstringext.h"
#include "sys/sync.h"
#include "hls-file.h"
#include "hls-param.h"

#define MAX_NAME 64

#if defined(OS_WINDOWS)
typedef __int64 int64_t;
#endif

struct hls_live_t
{
	struct list_head link;
    
    int opened; // 1-opened, 0-don't open

	long refcnt;
	locker_t locker;
	char name[MAX_NAME];

	unsigned int m3u8seq; // EXT-X-MEDIA-SEQUENCE

	struct hls_file_t *file; // temporary file
	struct hls_file_t *files[HLS_FILE_NUM];
	unsigned int file_count;

	void* ts;
    int64_t pts;
	time64_t rtime; // last read time
	time64_t wtime; // last write time
    
    unsigned char *vbuffer;
};

int hls_live_init();
void hls_live_cleanup();

/// create/destroy live object
struct hls_live_t* hls_live_fetch(const char* name);
int hls_live_release(struct hls_live_t* live);

/// read m3u8 file
/// @param[out] m3u8 string
/// @return 0-ok, other-error
int hls_live_m3u8(struct hls_live_t* live, char* m3u8);

/// read ts file
/// @param[in] file sequence number
struct hls_file_t* hls_live_file(struct hls_live_t* live, char* file);

/// write ts packet
/// @param[in] data ts packet
/// @param[in] bytes packet size in bytes
/// @param[in] stream packet stream id(H.264/AAC)
/// @return 0-ok, other-error
int hls_live_input(struct hls_live_t* live, const void* data, size_t bytes, int stream);


#endif /* !_hls_live_h_ */
