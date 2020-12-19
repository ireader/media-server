#ifndef _webm_internal_h_
#define _webm_internal_h_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "webm-buffer.h"

struct webm_segment_seek_t
{
	void* id;
	uint64_t pos;
};

struct webm_segment_info_t
{
	char* uid;
	uint64_t date; // utc
	uint32_t timescale;
	double duration;
};

struct webm_video_track_t
{
	int alpha;
	unsigned int width;
	unsigned int height;
	unsigned int aspect_ratio_type;

	double gamma;
	double fps;
};

struct webm_audio_track_t
{
	double sampling; // frequency
	unsigned int channels;
	unsigned int bits;
};

struct webm_segment_track_t
{
	uint64_t number;
	uint64_t uid;
	unsigned int type;
	int enable;
	int lacing;

	uint64_t duration;
	double timescale; // default 1.0
	int64_t offset;

	char* name;
	char* lang; // default eng
	char* codec;
	void* codec_extra;

	union
	{
		struct webm_video_track_t video;
		struct webm_audio_track_t audio;
	} u;
};

struct webm_segment_simple_block_t
{
	uint64_t timestamp;
	uint64_t position;
	uint64_t prev_size;
};

struct webm_segment_cluster_t
{
	uint64_t timestamp;
	uint64_t position;
	uint64_t prev_size;

	struct webm_segment_simple_block_t* blocks;
	size_t count, capacity;
};

struct webm_segment_chapter_t
{
	uint64_t uid;
	int flag_hidden;
	int flag_default;
	int flag_ordered;
};

struct webm_segment_tag_simple_t
{
	char* name;
	char* lang;
	char* string;
};

struct webm_segment_tag_t
{
	char* target_type;
	uint64_t target_type_value;

	struct webm_segment_tag_simple_t* simples;
	size_t count, capacity;
};

struct webm_segment_cue_position_t
{
	uint64_t track;
	uint64_t cluster;
	uint64_t relative;
	uint64_t duration;

	uint64_t block; // default 1
};

struct webm_segment_cue_t
{
	uint64_t time;

	struct webm_segment_cue_position_t* positions;
	size_t count, capacity;
};

struct webm_track_t
{
	int id;
};

struct webm_t
{
	struct webm_track_t* tracks;
	int track_count;
};

#endif /* !_webm_internal_h_ */
