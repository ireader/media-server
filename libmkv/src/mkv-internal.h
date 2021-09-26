#ifndef _mkv_internal_h_
#define _mkv_internal_h_

#include "ebml.h"
#include "mkv-buffer.h"
#include "mkv-format.h"
#include "mkv-ioutil.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

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

#define EBML_ID_EBML		0x1A45DFA3
#define EBML_ID_SEGMENT		0x18538067
#define EBML_ID_SEEK		0x114D9B74
#define EBML_ID_INFO		0x1549A966
#define EBML_ID_TRACKS		0x1654AE6B
#define EBML_ID_CLUSTER		0x1F43B675
#define EBML_ID_CUES		0x1C53BB6B
#define EBML_ID_TAGS		0x1254C367
#define EBML_ID_CHAPTERS	0x1043A770
#define EBML_ID_ATTACHMENTS	0x1941A469
#define EMLB_ID_VOID		0xEC

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
	int64_t attachments;
	int64_t tags;
	int64_t cluster;
	int64_t cues;
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
	uint32_t bytes;
	uint64_t offset;
	void* data;
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
	uint64_t timestamp; // Absolute timestamp according to the Segment time base.

	uint64_t block; // default 1
	int flag_codec_state; // codec state, default 0
};

struct mkv_cue_t
{
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

	int64_t first_ts;
	int64_t last_ts;
	int64_t sample_count;

	// Number of nanoseconds (not scaled via TimestampScale) per frame 
	// (frame in the Matroska sense -- one Element put into a (Simple)Block).
	uint64_t duration;
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
	char doc[16];

	struct mkv_track_t* tracks;
	int track_count;

	double duration;
	uint64_t timescale; // Timestamp scale in nanoseconds
	struct mkv_cluster_t *cluster;

	struct mkv_cue_t cue;
	struct mkv_segment_seek_t seek;

	struct mkv_sample_t* samples; // cache, read only
	int count, capacity;

	struct
	{
		int* raps; // keyframe index
		int count, capacity;
	} rap;
};

#define FREE(ptr) {void* p = (ptr); if(p) free(p); }

struct mkv_track_t* mkv_track_find(struct mkv_t* mkv, unsigned int id);
struct mkv_track_t* mkv_add_track(struct mkv_t* mkv);
int mkv_track_free(struct mkv_track_t* track);
int mkv_write_track(struct mkv_ioutil_t* io, struct mkv_track_t* track);

int mkv_add_video(struct mkv_track_t* track, enum mkv_codec_t codec, int width, int height, const void* extra_data, size_t extra_data_size);
int mkv_add_audio(struct mkv_track_t* track, enum mkv_codec_t codec, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size);
int mkv_add_subtitle(struct mkv_track_t* track, enum mkv_codec_t codec, const void* extra_data, size_t extra_data_size);

int mkv_cluster_simple_block_read(struct mkv_t* mkv, struct mkv_cluster_t* cluster, struct mkv_ioutil_t* io, int64_t bytes);
int mkv_cluster_simple_block_write(struct mkv_t* mkv, struct mkv_sample_t* sample, struct mkv_ioutil_t* io);

int mkv_cue_add(struct mkv_t* mkv, int track, int64_t timestamp, uint64_t cluster, uint64_t relative);
void mkv_write_cues(struct mkv_ioutil_t* io, struct mkv_t* mkv);

void mkv_write_size(const struct mkv_ioutil_t* io, uint64_t offset, uint32_t size);

#endif /* !_mkv_internal_h_ */
