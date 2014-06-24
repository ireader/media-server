#ifndef _hls_file_h_
#define _hls_file_h_

#include "time64.h"

#define BLOCK_SIZE (188*1000)

struct hls_block_t
{
	struct hls_block_t *next;
	void* bundle;
	void* ptr;
	int len;
};

struct hls_file_t
{
	struct hls_file_t *prev;
	struct hls_file_t *next;

	struct hls_block_t head;
	struct hls_block_t *tail;

	long refcnt;
	int seq; // filename
	int duration; // ms
	time64_t tcreate;
};

struct hls_file_t* hls_file_open();
int hls_file_close(struct hls_file_t* file);
int hls_file_write(struct hls_file_t* file, const void* packet, int bytes);

#endif /* !_hls_file_h_ */
