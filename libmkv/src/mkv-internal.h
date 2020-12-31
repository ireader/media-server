#ifndef _mkv_internal_h_
#define _mkv_internal_h_

#include "mkv-buffer.h"
#include "mkv-format.h"
#include "mkv-ioutil.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

enum
{
	MKV_TRACK_VIDEO		= 1,
	MKV_TRACK_AUDIO		= 2,
	MKV_TRACK_COMPLEX	= 3,
	MKV_TRACK_LOGO		= 16,
	MKV_TRACK_SUBTITLE	= 17,
	MKV_TRACK_BUTTONS	= 18,
	MKV_TRACK_CONTROL	= 32,
	MKV_TRACK_METADATA	= 33,
};

struct ebml_binary_t
{
	void* ptr;
	size_t len;
};

struct mkv_segment_seek_t
{
	int64_t info;
	int64_t tracks;
	int64_t chapters;
	int64_t cluster;
	int64_t cues;
	int64_t attachments;
	int64_t tags;
};

struct mkv_segment_info_t
{
	struct ebml_binary_t uid;
	uint64_t date; // utc
	uint64_t timescale;
	double duration;
};

struct mkv_video_track_t
{
	int alpha;
	unsigned int width;
	unsigned int height;
	unsigned int aspect_ratio_type;

	double gamma;
	double fps;
};

struct mkv_audio_track_t
{
	double sampling; // frequency
	unsigned int channels;
	unsigned int bits;
};

struct mkv_block_addition_t
{
	uint64_t id;
	struct ebml_binary_t addition;
};

struct mkv_block_slice_t
{
	uint64_t lace;
	uint64_t frame;
	uint64_t addition;
	uint64_t delay;
	uint64_t duration;
};

struct mkv_block_group_t
{
	struct ebml_binary_t block;

	uint64_t duration;
	int codec_state;

	struct mkv_block_addition_t* additions;
	size_t addition_count, addition_capacity;

	struct mkv_block_slice_t* slices;
	size_t slice_count, slice_capacity;
};

struct mkv_sample_t
{
	int track;
	int flags; // 1-keyframe
	int64_t dts;
	int64_t pts;
	uint64_t bytes;
	uint64_t offset;
};

struct mkv_cluster_t
{
	uint64_t timestamp;
	uint64_t position;
	uint64_t prev_size;

	struct mkv_block_group_t* groups;
	size_t count, capacity;
};

struct mkv_chapter_t
{
	uint64_t uid;
	int flag_hidden;
	int flag_default;
	int flag_ordered;
};

struct mkv_tag_simple_t
{
	char* name;
	char* lang;
	char* string;
};

struct mkv_tag_t
{
	char* target_type;
	uint64_t target_type_value;

	struct mkv_tag_simple_t* simples;
	size_t count, capacity;
};

struct mkv_cue_position_t
{
	uint64_t track;
	uint64_t cluster;
	uint64_t relative;
	uint64_t duration;

	uint64_t block; // default 1
	int flag_codec_state; // codec state, default 0
};

struct mkv_cue_t
{
	uint64_t time;

	struct mkv_cue_position_t* positions;
	size_t count, capacity;
};

struct mkv_track_t
{
	uint64_t uid;
	unsigned int id;
	unsigned int media;
	int flag_enabled;
	int flag_default;
	int flag_lacing;
	int flag_forced;

	uint64_t duration;
	double timescale; // default 1.0
	int64_t offset;

	enum mkv_codec_t codecid;
	char* name;
	char* lang; // default eng
	struct ebml_binary_t codec_extra;

	union
	{
		struct mkv_video_track_t video;
		struct mkv_audio_track_t audio;
	} u;
};

struct mkv_t
{
	struct mkv_track_t _track3[3]; // default
	struct mkv_track_t* tracks;
	int track_count, track_capacity;

	uint64_t timescale;
	struct mkv_cluster_t cluster;

	struct mkv_sample_t* samples;
	size_t count, capacity;
};

#define FREE(ptr) {void* p = (ptr); if(p) free(p); }

const char* mkv_codec_find_name(enum mkv_codec_t codec);
enum mkv_codec_t mkv_codec_find_id(const char* name);

struct mkv_track_t* mkv_track_find(struct mkv_t* mkv, int id);
int mkv_track_free(struct mkv_track_t* track);

int mkv_cluster_simple_block_read(struct mkv_t* mkv, struct mkv_cluster_t* cluster, struct mkv_ioutil_t* io, size_t bytes);

#endif /* !_mkv_internal_h_ */
