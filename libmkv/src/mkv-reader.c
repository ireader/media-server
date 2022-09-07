// https://www.matroska.org/technical/elements.html
// https://github.com/cellar-wg/ebml-specification/blob/master/specification.markdown

#include "mkv-reader.h"
#include "mkv-internal.h"
#include "mkv-ioutil.h"
#include "ebml.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

//#define MKV_PRINT_ELEMENT 1

//#define MKV_LIVE_STREAMING 1

#ifndef offsetof
#define offsetof(s, m)   (size_t)&(((s*)0)->m)
#endif

struct mkv_reader_t
{
	struct mkv_t mkv;
	struct mkv_ioutil_t io;

	struct ebml_header_t ebml;
	int64_t cue_time;
	uint64_t seek_id;

	uint64_t offset; // sample offset
	int segments; // only 1

	//struct mkv_segment_seek_t* seeks;
	//size_t seek_count, seek_capacity;

	struct mkv_segment_info_t* infos;
	size_t info_count, info_capacity;

	struct mkv_cluster_t* clusters;
	size_t cluster_count, cluster_capacity;

	struct mkv_chapter_t* chapters;
	size_t chapter_count, chapter_capacity;

	struct mkv_tag_t* tags;
	size_t tag_count, tag_capacity;
};

struct mkv_element_node_t;
struct mkv_element_t
{
	uint32_t id;
	enum ebml_element_type_t type;
	int level;
    
#if defined(DEBUG) || defined(_DEBUG)
	const char* name;
#endif

	int (*parse)(struct mkv_reader_t* reader, struct mkv_element_node_t* node);
	size_t offset;
};

struct mkv_element_node_t
{
	const struct mkv_element_node_t* parent;

	struct mkv_element_t* e;
	void* ptr;
	uint32_t id;
	int64_t size;

	uint64_t off; // offset at id
	uint64_t head; // head size
};

static int ebml_value_parse_bool(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	int* v;
	assert(node->parent);
	v = (int*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(EBML_TYPE_UINT == node->e->type);
	assert(0 <= node->size && node->size <= 8);
	*v = mkv_buffer_read_uint(&reader->io, (int)node->size) ? 1 : 0;
	return mkv_buffer_error(&reader->io);
}

static int ebml_value_parse_int(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	int* v;
	assert(node->parent);
	v = (int*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(EBML_TYPE_INT == node->e->type);
	assert(0 <= node->size && node->size <= 8);
	*v = (int)mkv_buffer_read_int(&reader->io, (int)node->size);
	return mkv_buffer_error(&reader->io);
}

static int ebml_value_parse_uint(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	unsigned int* v;
	assert(node->parent);
	v = (unsigned int*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(EBML_TYPE_UINT == node->e->type);
	assert(0 <= node->size && node->size <= 8);
	*v = (unsigned int)mkv_buffer_read_uint(&reader->io, (int)node->size);
	return mkv_buffer_error(&reader->io);
}

static int ebml_value_parse_int64(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	int64_t* v;
	assert(node->parent);
	v = (int64_t*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(EBML_TYPE_INT == node->e->type);
	assert(0 <= node->size && node->size <= 8);
	*v = mkv_buffer_read_int(&reader->io, (int)node->size);
	return mkv_buffer_error(&reader->io);
}

static int ebml_value_parse_uint64(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	uint64_t* v;
	assert(node->parent);
	v = (uint64_t*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(EBML_TYPE_UINT == node->e->type);
	assert(0 <= node->size && node->size <= 8);
	*v = mkv_buffer_read_uint(&reader->io, (int)node->size);
	return mkv_buffer_error(&reader->io);
}

static int ebml_value_parse_double(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	double* v;
	int64_t r;
	assert(node->parent);
	v = (double*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(EBML_TYPE_FLOAT == node->e->type);
	assert(0 == node->size || 4 == node->size || 8 == node->size);
	r = mkv_buffer_read_uint(&reader->io, (int)node->size);
	*v = 8 == node->size ? *(double*)&r : *(float*)&r;
	return mkv_buffer_error(&reader->io);
}

static int ebml_value_parse_binary(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	struct ebml_binary_t* v;
	assert(node->parent);
	v = (struct ebml_binary_t*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(EBML_TYPE_STRING == node->e->type || EBML_TYPE_UTF8 == node->e->type || EBML_TYPE_BINARY == node->e->type);

	v->len = node->size;
	v->ptr = malloc(node->size + 1);
	if (!v->ptr)
		return -ENOMEM;
	((char*)v->ptr)[v->len] = 0;
	mkv_buffer_read(&reader->io, v->ptr, v->len);
	return mkv_buffer_error(&reader->io);
}

static int ebml_value_parse_string(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	void** v;
	assert(node->parent);
	v = (void**)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(EBML_TYPE_STRING == node->e->type || EBML_TYPE_UTF8 == node->e->type || EBML_TYPE_BINARY == node->e->type);

	*v = malloc(node->size + 1);
	if (!*v)
		return -ENOMEM;
	((char*)*v)[node->size] = 0;
	mkv_buffer_read(&reader->io, *v, node->size);
	return mkv_buffer_error(&reader->io);
}

static int ebml_value_parse_utf8(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	return ebml_value_parse_string(reader, node);
}

static int ebml_value_parse_date(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	uint64_t* v;
	assert(node->parent);
	v = (uint64_t*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(EBML_TYPE_DATE == node->e->type);
	assert(0 == node->size || 8 == node->size);
	mkv_buffer_read(&reader->io, v, node->size);
	return mkv_buffer_error(&reader->io);
}

static int ebml_default_master_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	node->ptr = node->parent ? node->parent->ptr : NULL; // inherit
	(void)reader;
	return 0; // nothing to do
}

static int ebml_header_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	assert(NULL == node->parent);
	node->ptr = &reader->ebml;
	return 0; // nothing to do
}

static int mkv_segment_seek_id_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	assert(node->size <= 4);
	reader->seek_id = mkv_buffer_read_uint(&reader->io, (int)node->size);
	return 0; // nothing to do
}

static int mkv_segment_seek_pos_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	size_t i;
	uint64_t pos;
//	const char* names[] = { "Info", "Tracks", "Chapters", "Attachments", "Tags", "Cluster", "Cues" };
	const uint64_t ids[] = { EBML_ID_INFO, EBML_ID_TRACKS, EBML_ID_CHAPTERS, EBML_ID_ATTACHMENTS, EBML_ID_TAGS, EBML_ID_CLUSTER, EBML_ID_CUES };
	int64_t* offsets[] = { &reader->mkv.seek.info, &reader->mkv.seek.tracks, &reader->mkv.seek.chapters, &reader->mkv.seek.attachments, &reader->mkv.seek.tags, &reader->mkv.seek.cluster, &reader->mkv.seek.cues };
	
	assert(node->size <= 8);
	pos = mkv_buffer_read_uint(&reader->io, (int)node->size);

	for (i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
	{
		if (ids[i] == reader->seek_id)
		{
			//printf("seek id: %s, pos: %" PRIu64 "\n", names[i], pos);
			*offsets[i] = pos;
			return 0;
		}
	}

	//printf("seek id: 0x%X, pos: %" PRIu64 "\n", (unsigned int)reader->seek_id, pos);
	return 0; // nothing to do
}

static int mkv_realloc(void** ptr, size_t count, size_t *capacity, size_t size, size_t inc)
{
	void* p;
	if (count >= *capacity)
	{
		inc += *capacity * 2;
		p = realloc(*ptr, size * inc);
		if (!p)
			return -ENOMEM;

		memset((uint8_t*)p + size * count, 0, size * (inc - count));
		*ptr = p;
		*capacity = inc;
	}
	return 0;
}

static int mkv_segment_info_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	struct mkv_segment_info_t* info;
	if (0 != mkv_realloc((void**)&reader->infos, reader->info_count, &reader->info_capacity, sizeof(struct mkv_segment_info_t), 4))
		return -ENOMEM;
	
	info = &reader->infos[reader->info_count++];
	info->timescale = 1000000;

	node->ptr = info;
	return 0; // nothing to do
}

static int mkv_segment_track_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	struct mkv_track_t* track;
	track = mkv_add_track(&reader->mkv);
	if(!track)
		return -ENOMEM;

	track->flag_enabled = 1;
	track->flag_default = 1;
	track->flag_forced = 0;
	track->flag_lacing = 1;
	//track->lang = "eng";

	reader->mkv.track_count++;
	node->ptr = track;
	return 0; // nothing to do
}

static int mkv_segment_chapter_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	if (0 != mkv_realloc((void**)&reader->chapters, reader->chapter_count, &reader->chapter_capacity, sizeof(struct mkv_chapter_t), 4))
		return -ENOMEM;

	node->ptr = &reader->chapters[reader->chapter_count++];
	return 0; // nothing to do
}

static int mkv_segment_tag_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	if (0 != mkv_realloc((void**)&reader->tags, reader->tag_count, &reader->tag_capacity, sizeof(struct mkv_tag_t), 4))
		return -ENOMEM;

	node->ptr = &reader->tags[reader->tag_count++];
	return 0; // nothing to do
}

static int ebml_segment_tag_simple_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	struct mkv_tag_t* tag;
	tag = (struct mkv_tag_t*)node->parent->ptr;

	if (0 != mkv_realloc((void**)&tag->simples, tag->count, &tag->capacity, sizeof(struct mkv_tag_simple_t), 4))
		return -ENOMEM;

	node->ptr = &tag->simples[tag->count++];
	(void)reader;
	return 0; // nothing to do
}

static int mkv_segment_cue_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	node->ptr = reader;
	return 0; // nothing to do
}

static int mkv_segment_cue_position_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	struct mkv_cue_t* cue;
	struct mkv_cue_position_t* pt;
	//cue = (struct mkv_cue_t*)node->parent->ptr;
	cue = &reader->mkv.cue;

	if (0 != mkv_realloc((void**)&cue->positions, cue->count, &cue->capacity, sizeof(struct mkv_cue_position_t), 4))
		return -ENOMEM;

	pt = &cue->positions[cue->count++];
	pt->block = 1;
	pt->flag_codec_state = 0;
	pt->timestamp = reader->cue_time; // save cue time

	node->ptr = pt;
	return 0; // nothing to do
}

static int mkv_segment_cluster_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	struct mkv_cluster_t* cluster;
	if (0 != mkv_realloc((void**)&reader->clusters, reader->cluster_count, &reader->cluster_capacity, sizeof(struct mkv_cluster_t), 4))
		return -ENOMEM;

	cluster = &reader->clusters[reader->cluster_count++];
	cluster->position = node->off;

	reader->mkv.cluster = cluster;
	node->ptr = cluster;

#if !defined(MKV_LIVE_STREAMING)
	if(node->size > 0)
		mkv_buffer_skip(&reader->io, node->size); // nothing to do
#endif
	return node->size > 0 ? 0 : 1; // eof
}

static int mkv_segment_cluster_block_group_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	struct mkv_cluster_t* cluster;
	struct mkv_block_group_t* group;
	//cluster = (struct mkv_cluster_t*)node->parent->ptr;
	cluster = reader->mkv.cluster; // seek to block, no parent

	if (0 != mkv_realloc((void**)&cluster->groups, cluster->count, &cluster->capacity, sizeof(struct mkv_block_group_t), 4))
		return -ENOMEM;

	group = &cluster->groups[cluster->count++];
	node->ptr = group;
	return 0;
}

static int mkv_segment_cluster_block_group_addition_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	struct mkv_block_group_t* group;
	group = (struct mkv_block_group_t*)node->parent->ptr;

	if (0 != mkv_realloc((void**)&group->additions, group->addition_count, &group->addition_capacity, sizeof(struct mkv_block_addition_t), 4))
		return -ENOMEM;

	node->ptr = &group->additions[group->addition_count++];
	(void)reader;
	return 0;
}

static int mkv_segment_cluster_block_slice_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	struct mkv_block_group_t* group;
	group = (struct mkv_block_group_t*)node->parent->ptr;

	if (0 != mkv_realloc((void**)&group->slices, group->slice_count, &group->slice_capacity, sizeof(struct mkv_block_slice_t), 4))
		return -ENOMEM;

	node->ptr = &group->slices[group->slice_count++];
	(void)reader;
	return 0;
}

static int mkv_segment_cluster_block_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	node->ptr = reader->mkv.cluster;
	return 0;
}

static int mkv_segment_cluster_simple_block_parse(struct mkv_reader_t* reader, struct mkv_element_node_t* node)
{
	int r;
#if defined(MKV_LIVE_STREAMING)
	uint64_t pos;
#endif
	struct mkv_cluster_t* cluster;
	if (node->size < 0)
		return 0; // eof

	//cluster = (struct mkv_cluster_t*)node->parent->ptr;
	cluster = reader->mkv.cluster; // seek to block, no parent
	reader->mkv.timescale = reader->info_count > 0 ? reader->infos[0].timescale : 1000000; // for sample duration
	r = mkv_cluster_simple_block_read(&reader->mkv, cluster, &reader->io, node->size);

#if defined(MKV_LIVE_STREAMING)
	pos = mkv_buffer_tell(&reader->io);
	assert(node->off + node->head + node->size >= pos);
	mkv_buffer_skip(&reader->io, node->off + node->head + node->size - pos);
	return r; 
#else
	return 0 == r ? 1 : 0; // per block
#endif
}

#if defined(DEBUG) || defined(_DEBUG)
#define ELEMENT(id, type, level, name, parse, offset) {id, type, level, name, parse, offset}
#else
#define ELEMENT(id, type, level, name, parse, offset) {id, type, level, parse, offset}
#endif

static struct mkv_element_t s_elements[] = {
	// Global
	ELEMENT(0xBF,		EBML_TYPE_BINARY,	-1,	"CRC-32",					NULL, 0			), // CRC-32 Element, length 4
	ELEMENT(0xEC,		EBML_TYPE_BINARY,	-1,	"Void",						NULL, 0			), // Void Element

	// EBML
	ELEMENT(0x1A45DFA3,	EBML_TYPE_MASTER,	0,	"EBML",			            ebml_header_parse,      0), // EBML
	ELEMENT(0x4286,		EBML_TYPE_UINT,		1,	"EBMLVersion",              ebml_value_parse_uint,  offsetof(struct ebml_header_t, version)), // EBMLVersion, default 1
	ELEMENT(0x42F7,		EBML_TYPE_UINT,		1,	"EBMLReadVersion",          ebml_value_parse_uint,  offsetof(struct ebml_header_t, read_version)), // EBMLReadVersion, default 1
	ELEMENT(0x42F2,		EBML_TYPE_UINT,		1,	"EBMLMaxIDLength",          ebml_value_parse_uint,  offsetof(struct ebml_header_t, max_id_length)), // EBMLMaxIDLength, default 4
	ELEMENT(0x42F3,		EBML_TYPE_UINT,		1,	"EBMLMaxSizeLength",        ebml_value_parse_uint,  offsetof(struct ebml_header_t, max_size_length)), // EBMLMaxSizeLength, default 8
	ELEMENT(0x4282,		EBML_TYPE_STRING,	1,	"DocType",                  ebml_value_parse_string,offsetof(struct ebml_header_t, doc_type)), // DocType
	ELEMENT(0x4287,		EBML_TYPE_UINT,		1,	"DocTypeVersion",           ebml_value_parse_uint,  offsetof(struct ebml_header_t, doc_type_version)), // DocTypeVersion, default 1
	ELEMENT(0x4285,		EBML_TYPE_UINT,		1,	"DocTypeReadVersion",       ebml_value_parse_uint,  offsetof(struct ebml_header_t, doc_type_read_version)), // DocTypeReadVersion, default 1
	ELEMENT(0x4281,		EBML_TYPE_MASTER,	1,	"DocTypeExtension",         NULL,                   0), // DocTypeExtension
	ELEMENT(0x4283,		EBML_TYPE_STRING,	2,	"DocTypeExtensionName",     NULL,                   0), // DocTypeExtensionName
	ELEMENT(0x4284,		EBML_TYPE_UINT,		2,	"DocTypeExtensionVersion",  NULL,                   0), // DocTypeExtensionVersion

	// Segment
	ELEMENT(0x18538067,	EBML_TYPE_MASTER,   0,	"Segment",					ebml_default_master_parse,	0), // Segment

	// Cluster
	ELEMENT(0x1F43B675,	EBML_TYPE_MASTER,	1,	"Cluster",					mkv_segment_cluster_parse,  0), // Segment/Cluster [mult]

	// Meta Seek Information
	ELEMENT(0x114D9B74,	EBML_TYPE_MASTER,	1,	"SeekHead",					ebml_default_master_parse,  0), // Segment/SeekHead [mult]
	ELEMENT(0x4DBB,		EBML_TYPE_MASTER,	2,	"Seek",						ebml_default_master_parse,  0), // Segment/SeekHead/Seek [mult]
	ELEMENT(0x53AB,		EBML_TYPE_BINARY,	3,	"SeekID",					mkv_segment_seek_id_parse,	0), // Segment/SeekHead/Seek/SeekID
	ELEMENT(0x53AC,		EBML_TYPE_UINT,		3,	"SeekPosition",				mkv_segment_seek_pos_parse,	0), // Segment/SeekHead/Seek/SeekPosition

	// Segment Information
	ELEMENT(0x1549A966,	EBML_TYPE_MASTER,	1,	"Info",						mkv_segment_info_parse, 0), // Segment/Info [mult]
	ELEMENT(0x73A4,		EBML_TYPE_BINARY,	2,	"SegmentUID",				ebml_value_parse_binary,offsetof(struct mkv_segment_info_t, uid)), // Segment/Info/SegmentUID
	ELEMENT(0x7384,		EBML_TYPE_UTF8,		2,	"SegmentFilename",			NULL, 0), // Segment/Info/SegmentFilename
	ELEMENT(0x3CB923,	EBML_TYPE_BINARY,	2,	"PrevUID",					NULL, 0), // Segment/Info/PrevUID
	ELEMENT(0x3C83AB,	EBML_TYPE_UTF8,		2,	"PrevFilename",				NULL, 0), // Segment/Info/PrevFilename
	ELEMENT(0x3EB923,	EBML_TYPE_BINARY,	2,	"NextUID",					NULL, 0), // Segment/Info/NextUID
	ELEMENT(0x3E83BB,	EBML_TYPE_UTF8,		2,	"NextFilename",				NULL, 0), // Segment/Info/NextFilename
    ELEMENT(0x4444,		EBML_TYPE_BINARY,	2,	"SegmentFamily",			NULL, 0), // Segment/Info/SegmentFamily [mult]
    ELEMENT(0x6924,		EBML_TYPE_MASTER,	2,	"ChapterTranslate",			NULL, 0), // Segment/Info/ChapterTranslate [mult]
    ELEMENT(0x69FC,		EBML_TYPE_UINT,		3,	"ChapterTranslateEditionUID",NULL, 0), // Segment/Info/ChapterTranslate/ChapterTranslateEditionUID [mult]
    ELEMENT(0x69BF,		EBML_TYPE_UINT,		3,	"ChapterTranslateCodec",	NULL, 0), // Segment/Info/ChapterTranslate/ChapterTranslateCodec
    ELEMENT(0x69A5,		EBML_TYPE_BINARY,	3,	"ChapterTranslateID",		NULL, 0), // Segment/Info/ChapterTranslate/ChapterTranslateID
    ELEMENT(0x2AD7B1,	EBML_TYPE_UINT,		2,	"TimestampScale",			ebml_value_parse_uint64,offsetof(struct mkv_segment_info_t, timescale)), // Segment/Info/TimestampScale, default 1000000
    ELEMENT(0x4489,		EBML_TYPE_FLOAT,	2,	"Duration",					ebml_value_parse_double,offsetof(struct mkv_segment_info_t, duration)), // Segment/Info/Duration
    ELEMENT(0x4461,		EBML_TYPE_DATE,		2,	"DateUTC",					ebml_value_parse_date,	offsetof(struct mkv_segment_info_t, date)), // Segment/Info/DateUTC
    ELEMENT(0x7BA9,		EBML_TYPE_UTF8,		2,	"Title",					NULL, 0), // Segment/Info/Title
    ELEMENT(0x4D80,		EBML_TYPE_UTF8,		2,	"MuxingApp",				NULL, 0), // Segment/Info/MuxingApp
    ELEMENT(0x5741,		EBML_TYPE_UTF8,		2,	"WritingApp",				NULL, 0), // Segment/Info/WritingApp

	// Track
    ELEMENT(0x1654AE6B,	EBML_TYPE_MASTER,	1,	"Tracks",					ebml_default_master_parse, 0), // Segment/Tracks
    ELEMENT(0xAE,		EBML_TYPE_MASTER,	2,	"TrackEntry",				mkv_segment_track_parse, 0), // Segment/Tracks/TrackEntry [mult]
    ELEMENT(0xD7,		EBML_TYPE_UINT,		3,	"TrackNumber",				ebml_value_parse_uint,	offsetof(struct mkv_track_t, id)), // Segment/Tracks/TrackEntry/TrackNumber
    ELEMENT(0x73C5,		EBML_TYPE_UINT,		3,	"TrackUID",					ebml_value_parse_uint64,offsetof(struct mkv_track_t, uid)), // Segment/Tracks/TrackEntry/TrackUID
    ELEMENT(0x83,		EBML_TYPE_UINT,		3,	"TrackType",				ebml_value_parse_uint,	offsetof(struct mkv_track_t, media)), // Segment/Tracks/TrackEntry/TrackType, [1-254]
    ELEMENT(0xB9,		EBML_TYPE_UINT,		3,	"FlagEnabled",				ebml_value_parse_bool,	offsetof(struct mkv_track_t, flag_enabled)), // Segment/Tracks/TrackEntry/FlagEnabled, default 1
    ELEMENT(0x88,		EBML_TYPE_UINT,		3,	"FlagDefault",				ebml_value_parse_bool,	offsetof(struct mkv_track_t, flag_default)), // Segment/Tracks/TrackEntry/FlagDefault, default 1
    ELEMENT(0x55AA,		EBML_TYPE_UINT,		3,	"FlagForced",				ebml_value_parse_bool,	offsetof(struct mkv_track_t, flag_forced)), // Segment/Tracks/TrackEntry/FlagForced, default 0
    ELEMENT(0x9C,		EBML_TYPE_UINT,		3,	"FlagLacing",				ebml_value_parse_bool,	offsetof(struct mkv_track_t, flag_lacing)), // Segment/Tracks/TrackEntry/FlagLacing, default 1
    ELEMENT(0x6DE7,		EBML_TYPE_UINT,		3,	"MinCache",					NULL, 0), // Segment/Tracks/TrackEntry/MinCache, default 0
    ELEMENT(0x6DF8,		EBML_TYPE_UINT,		3,	"MaxCache",					NULL, 0), // Segment/Tracks/TrackEntry/MaxCache
    ELEMENT(0x23E383,	EBML_TYPE_UINT,		3,	"DefaultDuration",			ebml_value_parse_uint64,offsetof(struct mkv_track_t, duration)), // Segment/Tracks/TrackEntry/DefaultDuration
    ELEMENT(0x234E7A,	EBML_TYPE_UINT,		3,	"DefaultDecodedFieldDuration", NULL, 0), // Segment/Tracks/TrackEntry/DefaultDecodedFieldDuration
//    ELEMENT(0x23314F,	EBML_TYPE_FLOAT,	3,	"TrackTimestampScale",		ebml_value_parse_double,offsetof(struct mkv_track_t, timescale)), // Segment/Tracks/TrackEntry/TrackTimestampScale, default 1.0
    ELEMENT(0x537F,		EBML_TYPE_INT,		3,	"TrackOffset",	ebml_value_parse_int64,    offsetof(struct mkv_track_t, offset)), // Segment/Tracks/TrackEntry/TrackOffset, default 0
    ELEMENT(0x55EE,		EBML_TYPE_UINT,		3,	"MaxBlockAdditionID", NULL, 0), // Segment/Tracks/TrackEntry/MaxBlockAdditionID, default 0
    ELEMENT(0x41E4,		EBML_TYPE_MASTER,	3,	"BlockAdditionMapping", NULL, 0), // Segment/Tracks/TrackEntry/BlockAdditionMapping [mult]
    ELEMENT(0x41F0,		EBML_TYPE_UINT,		4,	"BlockAddIDValue", NULL, 0), // Segment/Tracks/TrackEntry/BlockAdditionMapping/BlockAddIDValue
    ELEMENT(0x41A4,		EBML_TYPE_STRING,	4,	"BlockAddIDName", NULL, 0), // Segment/Tracks/TrackEntry/BlockAdditionMapping/BlockAddIDName
    ELEMENT(0x41E7,		EBML_TYPE_UINT,		4,	"BlockAddIDType", NULL, 0), // Segment/Tracks/TrackEntry/BlockAdditionMapping/BlockAddIDType
    ELEMENT(0x41ED,		EBML_TYPE_BINARY,	4,	"BlockAddIDExtraData", NULL, 0), // Segment/Tracks/TrackEntry/BlockAdditionMapping/BlockAddIDExtraData
    ELEMENT(0x536E,		EBML_TYPE_UTF8,		3,	"Name", NULL, 0), // Segment/Tracks/TrackEntry/Name
    ELEMENT(0x22B59C,	EBML_TYPE_STRING,	3,	"Language", ebml_value_parse_string,offsetof(struct mkv_track_t, lang)), // Segment/Tracks/TrackEntry/Language, default eng
    ELEMENT(0x22B59D,	EBML_TYPE_STRING,	3,	"LanguageIETF", ebml_value_parse_string,offsetof(struct mkv_track_t, lang)), // Segment/Tracks/TrackEntry/LanguageIETF
    ELEMENT(0x86,		EBML_TYPE_STRING,	3,	"CodecID", ebml_value_parse_string,offsetof(struct mkv_track_t, name)), // Segment/Tracks/TrackEntry/CodecID
    ELEMENT(0x63A2,		EBML_TYPE_BINARY,	3,	"CodecPrivate", ebml_value_parse_binary, offsetof(struct mkv_track_t, codec_extra)), // Segment/Tracks/TrackEntry/CodecPrivate
    ELEMENT(0x258688,	EBML_TYPE_UTF8,		3,	"CodecName", NULL, 0), // Segment/Tracks/TrackEntry/CodecName
    ELEMENT(0x7446,		EBML_TYPE_UINT,		3,	"AttachmentLink", NULL, 0), // Segment/Tracks/TrackEntry/AttachmentLink
    ELEMENT(0x3A9697,	EBML_TYPE_UTF8,		3,	"CodecSettings", NULL, 0), // Segment/Tracks/TrackEntry/CodecSettings
    ELEMENT(0x3B4040,	EBML_TYPE_STRING,	3,	"CodecInfoURL", NULL, 0), // Segment/Tracks/TrackEntry/CodecInfoURL [mult]
    ELEMENT(0x26B240,	EBML_TYPE_STRING,	3,	"CodecDownloadURL", NULL, 0), // Segment/Tracks/TrackEntry/CodecDownloadURL [mult]
    ELEMENT(0xAA,		EBML_TYPE_UINT,		3,	"CodecDecodeAll", NULL, 0), // Segment/Tracks/TrackEntry/CodecDecodeAll
    ELEMENT(0x6FAB,		EBML_TYPE_UINT,		3,	"TrackOverlay", NULL, 0), // Segment/Tracks/TrackEntry/TrackOverlay [mult]
    ELEMENT(0x56AA,		EBML_TYPE_UINT,		3,	"CodecDelay", NULL, 0), // Segment/Tracks/TrackEntry/CodecDelay
    ELEMENT(0x56BB,		EBML_TYPE_UINT,		3,	"SeekPreRoll", NULL, 0), // Segment/Tracks/TrackEntry/SeekPreRoll
    ELEMENT(0x6624,		EBML_TYPE_MASTER,	3,	"TrackTranslate", NULL, 0), // Segment/Tracks/TrackEntry/TrackTranslate [mult]
    ELEMENT(0x66FC,		EBML_TYPE_UINT,		4,	"TrackTranslateEditionUID", NULL, 0), // Segment/Tracks/TrackEntry/TrackTranslate/TrackTranslateEditionUID [mult]
    ELEMENT(0x66BF,		EBML_TYPE_UINT,		4,	"TrackTranslateCodec", NULL, 0), // Segment/Tracks/TrackEntry/TrackTranslate/TrackTranslateCodec
    ELEMENT(0x66A5,		EBML_TYPE_BINARY,	4,	"TrackTranslateTrackID", NULL, 0), // Segment/Tracks/TrackEntry/TrackTranslate/TrackTranslateTrackID
    ELEMENT(0xE0,		EBML_TYPE_MASTER,	3,	"Video", ebml_default_master_parse, 0), // Segment/Tracks/TrackEntry/Video
    ELEMENT(0x9A,		EBML_TYPE_UINT,		4,	"FlagInterlaced", NULL, 0), // Segment/Tracks/TrackEntry/Video/FlagInterlaced, default 0
    ELEMENT(0x9D,		EBML_TYPE_UINT,		4,	"FieldOrder", NULL, 0), // Segment/Tracks/TrackEntry/Video/FieldOrder, default 2
    ELEMENT(0x53B8,		EBML_TYPE_UINT,		4,	"StereoMode", NULL, 0), // Segment/Tracks/TrackEntry/Video/StereoMode, default 0
    ELEMENT(0x53C0,		EBML_TYPE_UINT,		4,	"AlphaMode", ebml_value_parse_bool,    offsetof(struct mkv_track_t, u.video.alpha)), // Segment/Tracks/TrackEntry/Video/AlphaMode, default 0
    ELEMENT(0x53B9,		EBML_TYPE_UINT,		4,	"OldStereoMode", NULL, 0), // Segment/Tracks/TrackEntry/Video/OldStereoMode
    ELEMENT(0xB0,		EBML_TYPE_UINT,		4,	"PixelWidth", ebml_value_parse_uint,    offsetof(struct mkv_track_t, u.video.width)), // Segment/Tracks/TrackEntry/Video/PixelWidth
    ELEMENT(0xBA,		EBML_TYPE_UINT,		4,	"PixelHeight", ebml_value_parse_uint,    offsetof(struct mkv_track_t, u.video.height)), // Segment/Tracks/TrackEntry/Video/PixelHeight
    ELEMENT(0x54AA,		EBML_TYPE_UINT,		4,	"PixelCropBottom", NULL, 0), // Segment/Tracks/TrackEntry/Video/PixelCropBottom, default 0
    ELEMENT(0x54BB,		EBML_TYPE_UINT,		4,	"PixelCropTop", NULL, 0), // Segment/Tracks/TrackEntry/Video/PixelCropTop, default 0
    ELEMENT(0x54CC,		EBML_TYPE_UINT,		4,	"PixelCropLeft", NULL, 0), // Segment/Tracks/TrackEntry/Video/PixelCropLeft, default 0
    ELEMENT(0x54DD,		EBML_TYPE_UINT,		4,	"PixelCropRight", NULL, 0), // Segment/Tracks/TrackEntry/Video/PixelCropRight, default 0
    ELEMENT(0x54B0,		EBML_TYPE_UINT,		4,	"DisplayWidth", NULL, 0), // Segment/Tracks/TrackEntry/Video/DisplayWidth
    ELEMENT(0x54BA,		EBML_TYPE_UINT,		4,	"DisplayHeight", NULL, 0), // Segment/Tracks/TrackEntry/Video/DisplayHeight
    ELEMENT(0x54B2,		EBML_TYPE_UINT,		4,	"DisplayUnit", NULL, 0), // Segment/Tracks/TrackEntry/Video/DisplayUnit, default 0
    ELEMENT(0x54B3,		EBML_TYPE_UINT,		4,	"AspectRatioType", ebml_value_parse_uint,    offsetof(struct mkv_track_t, u.video.aspect_ratio_type)), // Segment/Tracks/TrackEntry/Video/AspectRatioType, default 0
    ELEMENT(0x2EB524,	EBML_TYPE_BINARY,	4,	"ColourSpace", NULL, 0), // Segment/Tracks/TrackEntry/Video/ColourSpace
    ELEMENT(0x2FB523,	EBML_TYPE_FLOAT,	4,	"GammaValue", ebml_value_parse_double,offsetof(struct mkv_track_t, u.video.gamma)), // Segment/Tracks/TrackEntry/Video/GammaValue
    ELEMENT(0x2383E3,	EBML_TYPE_FLOAT,	4,	"FrameRate", ebml_value_parse_double,offsetof(struct mkv_track_t, u.video.fps)), // Segment/Tracks/TrackEntry/Video/FrameRate
    ELEMENT(0x55B0,		EBML_TYPE_MASTER,	4,	"Colour", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour
    ELEMENT(0x55B1,		EBML_TYPE_UINT,		5,	"MatrixCoefficients", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MatrixCoefficients, default 2
    ELEMENT(0x55B2,		EBML_TYPE_UINT,		5,	"BitsPerChannel", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/BitsPerChannel, default 0
    ELEMENT(0x55B3,		EBML_TYPE_UINT,		5,	"ChromaSubsamplingHorz", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/ChromaSubsamplingHorz
    ELEMENT(0x55B4,		EBML_TYPE_UINT,		5,	"ChromaSubsamplingVert", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/ChromaSubsamplingVert
    ELEMENT(0x55B5,		EBML_TYPE_UINT,		5,	"CbSubsamplingHorz", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/CbSubsamplingHorz
    ELEMENT(0x55B6,		EBML_TYPE_UINT,		5,	"CbSubsamplingVert", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/CbSubsamplingVert
    ELEMENT(0x55B7,		EBML_TYPE_UINT,		5,	"ChromaSitingHorz", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/ChromaSitingHorz, default 0
    ELEMENT(0x55B8,		EBML_TYPE_UINT,		5,	"ChromaSitingVert", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/ChromaSitingVert, default 0
    ELEMENT(0x55B9,		EBML_TYPE_UINT,		5,	"Range", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/Range, default 0
    ELEMENT(0x55BA,		EBML_TYPE_UINT,		5,	"TransferCharacteristics", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/TransferCharacteristics, default 2
    ELEMENT(0x55BB,		EBML_TYPE_UINT,		5,	"Primaries", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/Primaries, default 2
    ELEMENT(0x55BC,		EBML_TYPE_UINT,		5,	"MaxCLL", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MaxCLL
    ELEMENT(0x55BD,		EBML_TYPE_UINT,		5,	"MaxFALL", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MaxFALL
    ELEMENT(0x55D0,		EBML_TYPE_MASTER,	5,	"MasteringMetadata", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MasteringMetadata
    ELEMENT(0x55D1,		EBML_TYPE_FLOAT,    6,	"PrimaryRChromaticityX", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MasteringMetadata/PrimaryRChromaticityX
    ELEMENT(0x55D2,		EBML_TYPE_FLOAT,    6,	"PrimaryRChromaticityY", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MasteringMetadata/PrimaryRChromaticityY
    ELEMENT(0x55D3,		EBML_TYPE_FLOAT,    6,	"PrimaryGChromaticityX", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MasteringMetadata/PrimaryGChromaticityX
    ELEMENT(0x55D4,		EBML_TYPE_FLOAT,    6,	"PrimaryGChromaticityY", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MasteringMetadata/PrimaryGChromaticityY
    ELEMENT(0x55D5,		EBML_TYPE_FLOAT,    6,	"PrimaryBChromaticityX", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MasteringMetadata/PrimaryBChromaticityX
    ELEMENT(0x55D6,		EBML_TYPE_FLOAT,    6,	"PrimaryBChromaticityY", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MasteringMetadata/PrimaryBChromaticityY
    ELEMENT(0x55D7,		EBML_TYPE_FLOAT,    6,	"WhitePointChromaticityX", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MasteringMetadata/WhitePointChromaticityX
    ELEMENT(0x55D8,		EBML_TYPE_FLOAT,    6,	"WhitePointChromaticityY", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MasteringMetadata/WhitePointChromaticityY
    ELEMENT(0x55D9,		EBML_TYPE_FLOAT,    6,	"LuminanceMax", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MasteringMetadata/LuminanceMax
    ELEMENT(0x55DA,		EBML_TYPE_FLOAT,    6,	"LuminanceMin", NULL, 0), // Segment/Tracks/TrackEntry/Video/Colour/MasteringMetadata/LuminanceMin
    ELEMENT(0x7670,		EBML_TYPE_MASTER,	4,	"Projection", NULL, 0), // Segment/Tracks/TrackEntry/Video/Projection
    ELEMENT(0x7671,		EBML_TYPE_UINT,		5,	"ProjectionType", NULL, 0), // Segment/Tracks/TrackEntry/Video/ProjectionType
    ELEMENT(0x7672,		EBML_TYPE_BINARY,	5,	"ProjectionPrivate", NULL, 0), // Segment/Tracks/TrackEntry/Video/ProjectionPrivate
    ELEMENT(0x7673,		EBML_TYPE_FLOAT,	5,	"ProjectionPoseYaw", NULL, 0), // Segment/Tracks/TrackEntry/Video/ProjectionPoseYaw
    ELEMENT(0x7674,		EBML_TYPE_FLOAT,	5,	"ProjectionPosePitch", NULL, 0), // Segment/Tracks/TrackEntry/Video/ProjectionPosePitch
    ELEMENT(0x7675,		EBML_TYPE_FLOAT,	5,	"ProjectionPoseRoll", NULL, 0), // Segment/Tracks/TrackEntry/Video/ProjectionPoseRoll
    ELEMENT(0xE1,		EBML_TYPE_MASTER,	3,	"Audio", ebml_default_master_parse, 0), // Segment/Tracks/TrackEntry/Audio
    ELEMENT(0xB5,		EBML_TYPE_FLOAT,	4,	"SamplingFrequency", ebml_value_parse_double,offsetof(struct mkv_track_t, u.audio.sampling)), // Segment/Tracks/TrackEntry/Audio/SamplingFrequency, default 8000
    ELEMENT(0x78B5,		EBML_TYPE_FLOAT,	4,	"OutputSamplingFrequency", NULL, 0), // Segment/Tracks/TrackEntry/Audio/OutputSamplingFrequency
    ELEMENT(0x9F,		EBML_TYPE_UINT,		4,	"Channels", ebml_value_parse_uint,    offsetof(struct mkv_track_t, u.audio.channels)), // Segment/Tracks/TrackEntry/Audio/Channels, default 1
    ELEMENT(0x7D7B,		EBML_TYPE_BINARY,	4,	"ChannelPositions", ebml_value_parse_uint, 0), // Segment/Tracks/TrackEntry/Audio/ChannelPositions
    ELEMENT(0x6264,		EBML_TYPE_UINT,		4,	"BitDepth", ebml_value_parse_uint,    offsetof(struct mkv_track_t, u.audio.bits)), // Segment/Tracks/TrackEntry/Audio/BitDepth
    ELEMENT(0xE2,		EBML_TYPE_MASTER,	3,	"TrackOperation", NULL, 0), // Segment/Tracks/TrackEntry/TrackOperation
    ELEMENT(0xE3,		EBML_TYPE_MASTER,	4,	"TrackCombinePlanes", NULL, 0), // Segment/Tracks/TrackEntry/TrackOperation/TrackCombinePlanes
    ELEMENT(0xE4,		EBML_TYPE_MASTER,	5,	"TrackPlane", NULL, 0), // Segment/Tracks/TrackEntry/TrackOperation/TrackCombinePlanes/TrackPlane [mult]
    ELEMENT(0xE5,		EBML_TYPE_UINT,     6,	"TrackPlaneUID", NULL, 0), // Segment/Tracks/TrackEntry/TrackOperation/TrackCombinePlanes/TrackPlane/TrackPlaneUID
    ELEMENT(0xE6,		EBML_TYPE_UINT,     6,	"TrackPlaneType", NULL, 0), // Segment/Tracks/TrackEntry/TrackOperation/TrackCombinePlanes/TrackPlane/TrackPlaneType
    ELEMENT(0xE9,		EBML_TYPE_MASTER,	4,	"TrackJoinBlocks", NULL, 0), // Segment/Tracks/TrackEntry/TrackOperation/TrackJoinBlocks
    ELEMENT(0xED,		EBML_TYPE_UINT,		5,	"TrackJoinUID", NULL, 0), // Segment/Tracks/TrackEntry/TrackOperation/TrackJoinBlocks/TrackJoinUID
    ELEMENT(0xC0,		EBML_TYPE_UINT,		3,	"TrickTrackUID", NULL, 0), // Segment/Tracks/TrackEntry/TrickTrackUID // 	DivX trick track extensions
    ELEMENT(0xC1,		EBML_TYPE_BINARY,	3,	"TrickTrackSegmentUID", NULL, 0), // Segment/Tracks/TrackEntry/TrickTrackSegmentUID // 	DivX trick track extensions
    ELEMENT(0xC6,		EBML_TYPE_UINT,		3,	"TrickTrackFlag", NULL, 0), // Segment/Tracks/TrackEntry/TrickTrackFlag // 	DivX trick track extensions
    ELEMENT(0xC7,		EBML_TYPE_UINT,		3,	"TrickMasterTrackUID", NULL, 0), // Segment/Tracks/TrackEntry/TrickMasterTrackUID // 	DivX trick track extensions
    ELEMENT(0xC4,		EBML_TYPE_BINARY,	3,	"TrickMasterTrackSegmentUID", NULL, 0), // Segment/Tracks/TrackEntry/TrickMasterTrackSegmentUID // 	DivX trick track extensions
    ELEMENT(0x6D80,		EBML_TYPE_MASTER,	3,	"ContentEncodings", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings
    ELEMENT(0x6240,		EBML_TYPE_MASTER,	4,	"ContentEncoding", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding [mult]
    ELEMENT(0x5031,		EBML_TYPE_UINT,		5,	"ContentEncodingOrder", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentEncodingOrder, default 0
    ELEMENT(0x5032,		EBML_TYPE_UINT,		5,	"ContentEncodingScope", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentEncodingScope, default 1
    ELEMENT(0x5033,		EBML_TYPE_UINT,		5,	"ContentEncodingType", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentEncodingType, default 0
    ELEMENT(0x5034,		EBML_TYPE_MASTER,	5,	"ContentCompression", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentCompression
    ELEMENT(0x4254,		EBML_TYPE_UINT,     6,	"ContentCompAlgo", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentCompression/ContentCompAlgo, default 0
    ELEMENT(0x4255,		EBML_TYPE_BINARY,   6,	"ContentCompSettings", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentCompression/ContentCompSettings
    ELEMENT(0x5035,		EBML_TYPE_MASTER,	5,	"ContentEncryption", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentEncryption
    ELEMENT(0x47E1,		EBML_TYPE_UINT,     6,	"ContentEncAlgo", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentEncryption/ContentEncAlgo, default 0
    ELEMENT(0x47E2,		EBML_TYPE_BINARY,   6,	"ContentEncKeyID", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentEncryption/ContentEncKeyID
    ELEMENT(0x47E7,		EBML_TYPE_MASTER,   6,	"ContentEncAESSettings", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentEncryption/ContentEncAESSettings
    ELEMENT(0x47E8,		EBML_TYPE_UINT,     7,	"AESSettingsCipherMode", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentEncryption/ContentEncAESSettings/AESSettingsCipherMode
    ELEMENT(0x47E3,		EBML_TYPE_BINARY,   6,	"ContentSignature", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentEncryption/ContentSignature
    ELEMENT(0x47E4,		EBML_TYPE_BINARY,   6,	"ContentSigKeyID", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentEncryption/ContentSigKeyID
    ELEMENT(0x47E5,		EBML_TYPE_UINT,     6,	"ContentSigAlgo", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentEncryption/ContentSigAlgo
    ELEMENT(0x47E6,		EBML_TYPE_UINT,     6,	"ContentSigHashAlgo", NULL, 0), // Segment/Tracks/TrackEntry/ContentEncodings/ContentEncoding/ContentEncryption/ContentSigHashAlgo

	// Cueing Data
    ELEMENT(0x1C53BB6B,	EBML_TYPE_MASTER,	1,	"Cues", ebml_default_master_parse, 0), // Segment/Cues
    ELEMENT(0xBB,		EBML_TYPE_MASTER,	2,	"CuePoint", mkv_segment_cue_parse, 0), // Segment/Cues/CuePoint [mult]
    ELEMENT(0xB3,		EBML_TYPE_UINT,		3,	"CueTime", ebml_value_parse_uint64,offsetof(struct mkv_reader_t, cue_time)), // Segment/Cues/CuePoint/CueTime
    ELEMENT(0xB7,		EBML_TYPE_MASTER,	3,	"CueTrackPositions", mkv_segment_cue_position_parse, 0), // Segment/Cues/CuePoint/CueTrackPositions [mult]
    ELEMENT(0xF7,		EBML_TYPE_UINT,		4,	"CueTrack", ebml_value_parse_uint64,offsetof(struct mkv_cue_position_t, track)), // Segment/Cues/CuePoint/CueTrackPositions/CueTrack
    ELEMENT(0xF1,		EBML_TYPE_UINT,		4,	"CueClusterPosition", ebml_value_parse_uint64,offsetof(struct mkv_cue_position_t, cluster) ), // Segment/Cues/CuePoint/CueTrackPositions/CueClusterPosition
    ELEMENT(0xF0,		EBML_TYPE_UINT,		4,	"CueRelativePosition", ebml_value_parse_uint64,offsetof(struct mkv_cue_position_t, relative) ), // Segment/Cues/CuePoint/CueTrackPositions/CueRelativePosition
    ELEMENT(0xB2,		EBML_TYPE_UINT,		4, 	"CueDuration", ebml_value_parse_uint64,offsetof(struct mkv_cue_position_t, duration) ), // Segment/Cues/CuePoint/CueTrackPositions/CueDuration
    ELEMENT(0x5378,		EBML_TYPE_UINT,		4,	"CueBlockNumber", ebml_value_parse_uint64,offsetof(struct mkv_cue_position_t, block) ), // Segment/Cues/CuePoint/CueTrackPositions/CueBlockNumber, default 1
    ELEMENT(0xEA,		EBML_TYPE_UINT,		4,	"CueCodecState", ebml_value_parse_bool,    offsetof(struct mkv_cue_position_t, flag_codec_state) ), // Segment/Cues/CuePoint/CueTrackPositions/CueCodecState, default 0
    ELEMENT(0xDB,		EBML_TYPE_MASTER,	4,	"CueReference", NULL, 0), // Segment/Cues/CuePoint/CueTrackPositions/CueReference [mult]
    ELEMENT(0x96,		EBML_TYPE_UINT,		5,	"CueRefTime", NULL, 0), // Segment/Cues/CuePoint/CueTrackPositions/CueReference/CueRefTime
    ELEMENT(0x97,		EBML_TYPE_UINT,		5,	"CueRefCluster", NULL, 0), // Segment/Cues/CuePoint/CueTrackPositions/CueReference/CueRefCluster
    ELEMENT(0x535F,		EBML_TYPE_UINT,		5,	"CueRefNumber", NULL, 0), // Segment/Cues/CuePoint/CueTrackPositions/CueReference/CueRefNumber, default 1
    ELEMENT(0xEB,		EBML_TYPE_UINT,		5,	"CueRefCodecState", NULL, 0), // Segment/Cues/CuePoint/CueTrackPositions/CueReference/CueRefCodecState, default 0

	// Attachment
    ELEMENT(0x1941A469,	EBML_TYPE_MASTER,	1,	"Attachments", NULL, 0), // Segment/Attachments
    ELEMENT(0x61A7,		EBML_TYPE_MASTER,	2,	"AttachedFile", NULL, 0), // Segment/Attachments/AttachedFile [mult]
    ELEMENT(0x467E,		EBML_TYPE_UTF8,		3,	"FileDescription", NULL, 0), // Segment/Attachments/AttachedFile/FileDescription
    ELEMENT(0x466E,		EBML_TYPE_UTF8,		3,	"FileName", NULL, 0), // Segment/Attachments/AttachedFile/FileName
    ELEMENT(0x4660,		EBML_TYPE_STRING,	3,	"FileMimeType", NULL, 0), // Segment/Attachments/AttachedFile/FileMimeType
    ELEMENT(0x465C,		EBML_TYPE_BINARY,	3,	"FileData", NULL, 0), // Segment/Attachments/AttachedFile/FileData
    ELEMENT(0x46AE,		EBML_TYPE_UINT,		3,	"FileUID", NULL, 0), // Segment/Attachments/AttachedFile/FileUID
    ELEMENT(0x4675,		EBML_TYPE_BINARY,	3,	"FileReferral", NULL, 0), // Segment/Attachments/AttachedFile/FileReferral
    ELEMENT(0x4661,		EBML_TYPE_UINT,		3,	"FileUsedStartTime", NULL, 0), // Segment/Attachments/AttachedFile/FileUsedStartTime
    ELEMENT(0x4662,		EBML_TYPE_UINT,		3,	"FileUsedEndTime", NULL, 0), // Segment/Attachments/AttachedFile/FileUsedEndTime

	// Chapters
    ELEMENT(0x1043A770,	EBML_TYPE_MASTER,	1,	"Chapters", ebml_default_master_parse, 0), // Segment/Chapters
    ELEMENT(0x45B9,		EBML_TYPE_MASTER,	2,	"EditionEntry", mkv_segment_chapter_parse, 0), // Segment/Chapters/EditionEntry [mult]
    ELEMENT(0x45BC,		EBML_TYPE_UINT,		3,	"EditionUID", ebml_value_parse_uint,    offsetof(struct mkv_chapter_t, uid)), // Segment/Chapters/EditionEntry/EditionUID
    ELEMENT(0x45BD,		EBML_TYPE_UINT,		3,	"EditionFlagHidden", ebml_value_parse_uint,    offsetof(struct mkv_chapter_t, flag_hidden)), // Segment/Chapters/EditionEntry/EditionFlagHidden, default 0
    ELEMENT(0x45DB,		EBML_TYPE_UINT,		3,	"EditionFlagDefault", ebml_value_parse_uint,    offsetof(struct mkv_chapter_t, flag_default)), // Segment/Chapters/EditionEntry/EditionFlagDefault, default 0
    ELEMENT(0x45DD,		EBML_TYPE_UINT,		3,	"EditionFlagOrdered", ebml_value_parse_uint,    offsetof(struct mkv_chapter_t, flag_ordered)), // Segment/Chapters/EditionEntry/EditionFlagOrdered, default 0
    ELEMENT(0xB6,		EBML_TYPE_MASTER,	3,	"ChapterAtom", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom [mult]
    ELEMENT(0x73C4,		EBML_TYPE_UINT,		4,	"ChapterUID", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterUID
    ELEMENT(0x5654,		EBML_TYPE_UTF8,		4,	"ChapterStringUID", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterStringUID
    ELEMENT(0x91,		EBML_TYPE_UINT,		4,	"ChapterTimeStart", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterTimeStart
    ELEMENT(0x92,		EBML_TYPE_UINT,		4,	"ChapterTimeEnd", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterTimeEnd
    ELEMENT(0x98,		EBML_TYPE_UINT,		4,	"ChapterFlagHidden", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterFlagHidden, default 0
    ELEMENT(0x4598,		EBML_TYPE_UINT,		4,	"ChapterFlagEnabled", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterFlagEnabled, default 1
    ELEMENT(0x6E67,		EBML_TYPE_BINARY,	4,	"ChapterSegmentUID", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterSegmentUID
    ELEMENT(0x6EBC,		EBML_TYPE_UINT,		4,	"ChapterSegmentEditionUID", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterSegmentEditionUID
    ELEMENT(0x63C3,		EBML_TYPE_UINT,		4,	"ChapterPhysicalEquiv", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterPhysicalEquiv
    ELEMENT(0x8F,		EBML_TYPE_MASTER,	4,	"ChapterTrack", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterTrack
    ELEMENT(0x89,		EBML_TYPE_UINT,		5,	"ChapterTrackUID", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterTrack/ChapterTrackUID [mult]
    ELEMENT(0x80,		EBML_TYPE_MASTER,	4,	"ChapterDisplay", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterDisplay [mult]
    ELEMENT(0x85,		EBML_TYPE_UTF8,		5,	"ChapString", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterDisplay/ChapString
    ELEMENT(0x437C,		EBML_TYPE_STRING,	5,	"ChapLanguage", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterDisplay/ChapLanguage [mult], default eng
    ELEMENT(0x437D,		EBML_TYPE_STRING,	5,	"ChapLanguageIETF", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterDisplay/ChapLanguageIETF [mult]
    ELEMENT(0x437E,		EBML_TYPE_STRING,	5,	"ChapCountry", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapterDisplay/ChapCountry [mult]
    ELEMENT(0x6944,		EBML_TYPE_MASTER,	4,	"ChapProcess", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapProcess [mult]
    ELEMENT(0x6955,		EBML_TYPE_UINT,		5,	"ChapProcessCodecID", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapProcess/ChapProcessCodecID
    ELEMENT(0x450D,		EBML_TYPE_BINARY,	5,	"ChapProcessPrivate", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapProcess/ChapProcessPrivate
    ELEMENT(0x6911,		EBML_TYPE_MASTER,	5,	"ChapProcessCommand", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapProcess/ChapProcessCommand [mult]
    ELEMENT(0x6922,		EBML_TYPE_UINT,     6,	"ChapProcessTime", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapProcess/ChapProcessComman/ChapProcessTime
    ELEMENT(0x6933,		EBML_TYPE_BINARY,   6,	"ChapProcessData", NULL, 0), // Segment/Chapters/EditionEntry/ChapterAtom/ChapProcess/ChapProcessComman/ChapProcessData

	// Tagging
    ELEMENT(0x1254C367,	EBML_TYPE_MASTER,	1,	"Tags", ebml_default_master_parse, 0), // Segment/Tags [mult]
    ELEMENT(0x7373,		EBML_TYPE_MASTER,	2,	"Tag", mkv_segment_tag_parse, 0), // Segment/Tags/Tag [mult]
    ELEMENT(0x63C0,		EBML_TYPE_MASTER,	3,	"Targets", NULL, 0), // Segment/Tags/Tag/Targets
    ELEMENT(0x68CA,		EBML_TYPE_UINT,		4,	"TargetTypeValue", NULL, 0), // Segment/Tags/Tag/Targets/TargetTypeValue, default 50
    ELEMENT(0x63CA,		EBML_TYPE_STRING,	4,	"TargetType", NULL, 0), // Segment/Tags/Tag/Targets/TargetType
    ELEMENT(0x63C5,		EBML_TYPE_UINT,		4,	"TagTrackUID", NULL, 0), // Segment/Tags/Tag/Targets/TagTrackUID [mult]
    ELEMENT(0x63C9,		EBML_TYPE_UINT,		4,	"TagEditionUID", NULL, 0), // Segment/Tags/Tag/Targets/TagEditionUID [mult]
    ELEMENT(0x63C4,		EBML_TYPE_UINT,		4,	"TagChapterUID", NULL, 0), // Segment/Tags/Tag/Targets/TagChapterUID [mult]
    ELEMENT(0x63C6,		EBML_TYPE_UINT,		4,	"TagAttachmentUID", NULL, 0), // Segment/Tags/Tag/Targets/TagAttachmentUID [mult]
    ELEMENT(0x67C8,		EBML_TYPE_MASTER,	3,	"SimpleTag", ebml_segment_tag_simple_parse, 0), // Segment/Tags/Tag/SimpleTag [mult]
    ELEMENT(0x45A3,		EBML_TYPE_UTF8,		4,	"TagName", ebml_value_parse_utf8,    offsetof(struct mkv_tag_simple_t, name)), // Segment/Tags/Tag/SimpleTag/TagName
    ELEMENT(0x447A,		EBML_TYPE_STRING,	4,	"TagLanguage", ebml_value_parse_string,offsetof(struct mkv_tag_simple_t, lang)), // Segment/Tags/Tag/SimpleTag/TagLanguage, default und
    ELEMENT(0x447B,		EBML_TYPE_STRING,	4,	"TagLanguageIETF", ebml_value_parse_string,offsetof(struct mkv_tag_simple_t, lang)), // Segment/Tags/Tag/SimpleTag/TagLanguageIETF
    ELEMENT(0x4484,		EBML_TYPE_UINT,		4,	"TagDefault", NULL, 0), // Segment/Tags/Tag/SimpleTag/TagDefault, default 1
    ELEMENT(0x4487,		EBML_TYPE_UTF8,		4,	"TagString", ebml_value_parse_utf8,    offsetof(struct mkv_tag_simple_t, string)), // Segment/Tags/Tag/SimpleTag/TagString
    ELEMENT(0x4485,		EBML_TYPE_BINARY,	4,	"TagBinary", NULL, 0), // Segment/Tags/Tag/SimpleTag/TagBinary

#if !defined(MKV_LIVE_STREAMING)
};

static struct mkv_element_t s_clusters[] = {
	// Segment
    ELEMENT(0x18538067,	EBML_TYPE_MASTER,	0,	"Segment",	ebml_default_master_parse,	0), // Segment

	// Cluster
    ELEMENT(0x1F43B675,	EBML_TYPE_MASTER,	1,	"Cluster", mkv_segment_cluster_block_parse, 0), // Segment/Cluster [mult]
#endif
    ELEMENT(0xE7,		EBML_TYPE_UINT,		2,	"Timestamp", ebml_value_parse_uint64,offsetof(struct mkv_cluster_t, timestamp)), // Segment/Cluster/Timestamp
    ELEMENT(0x5854,		EBML_TYPE_MASTER,	2,	"SilentTracks", NULL, 0), // Segment/Cluster/SilentTracks
    ELEMENT(0x58D7,		EBML_TYPE_UINT,		3,	"SilentTrackNumber", NULL, 0), // Segment/Cluster/SilentTracks/SilentTrackNumber [mult]
    ELEMENT(0xA7,		EBML_TYPE_UINT,		2,	"Position", ebml_value_parse_uint64,offsetof(struct mkv_cluster_t, position)), // Segment/Cluster/Position
    ELEMENT(0xAB,		EBML_TYPE_UINT,		2,	"PrevSize", ebml_value_parse_uint64,offsetof(struct mkv_cluster_t, prev_size)), // Segment/Cluster/PrevSize
    ELEMENT(0xA3,		EBML_TYPE_BINARY,	2,	"SimpleBlock", mkv_segment_cluster_simple_block_parse, 0), // Segment/Cluster/SimpleBlock [mult]
    ELEMENT(0xA0,		EBML_TYPE_MASTER,	2,	"BlockGroup",	mkv_segment_cluster_block_group_parse, 0), // Segment/Cluster/BlockGroup [mult]
    ELEMENT(0xA1,		EBML_TYPE_BINARY,	3,	"Block", ebml_value_parse_binary,offsetof(struct mkv_block_group_t, block)), // Segment/Cluster/BlockGroup/Block
    ELEMENT(0xA2,		EBML_TYPE_BINARY,	3,	"BlockVirtual", NULL, 0), // Segment/Cluster/BlockGroup/BlockVirtual
    ELEMENT(0x75A1,		EBML_TYPE_MASTER,	3,	"BlockAdditions", ebml_default_master_parse, 0), // Segment/Cluster/BlockGroup/BlockAdditions
    ELEMENT(0xA6,		EBML_TYPE_MASTER,	4,	"BlockMore", mkv_segment_cluster_block_group_addition_parse, 0), // Segment/Cluster/BlockGroup/BlockAdditions/BlockMore [mult]
    ELEMENT(0xEE,		EBML_TYPE_UINT,		5,	"BlockAddID", ebml_value_parse_uint64,offsetof(struct mkv_block_addition_t, id)), // Segment/Cluster/BlockGroup/BlockAdditions/BlockMore/BlockAddID
    ELEMENT(0xA5,		EBML_TYPE_BINARY,	5,	"BlockAdditional", ebml_value_parse_binary,offsetof(struct mkv_block_addition_t, addition)), // Segment/Cluster/BlockGroup/BlockAdditions/BlockMore/BlockAdditional
    ELEMENT(0x9B,		EBML_TYPE_UINT,		3,	"BlockDuration", ebml_value_parse_uint64,offsetof(struct mkv_block_group_t, duration)), // Segment/Cluster/BlockGroup/BlockDuration
    ELEMENT(0xFA,		EBML_TYPE_UINT,		3,	"ReferencePriority", NULL, 0), // Segment/Cluster/BlockGroup/ReferencePriority, default 0
    ELEMENT(0xFB,		EBML_TYPE_INT,		3,	"ReferenceBlock", NULL, 0), // Segment/Cluster/BlockGroup/ReferenceBlock [mult]
    ELEMENT(0xFD,		EBML_TYPE_INT,		3,	"ReferenceVirtual", NULL, 0), // Segment/Cluster/BlockGroup/ReferenceVirtual
    ELEMENT(0xA4,		EBML_TYPE_BINARY,	3,	"CodecState", NULL, 0), // Segment/Cluster/BlockGroup/CodecState
    ELEMENT(0x75A2,		EBML_TYPE_INT,		3,	"DiscardPadding", NULL, 0), // Segment/Cluster/BlockGroup/DiscardPadding
    ELEMENT(0x8E,		EBML_TYPE_MASTER,	3,	"Slices", ebml_default_master_parse, 0), // Segment/Cluster/BlockGroup/Slices
    ELEMENT(0xE8,		EBML_TYPE_MASTER,	4,	"TimeSlice", mkv_segment_cluster_block_slice_parse, 0), // Segment/Cluster/BlockGroup/Slices/TimeSlice [mult]
    ELEMENT(0xCC,		EBML_TYPE_UINT,		5,	"LaceNumber", ebml_value_parse_uint64,offsetof(struct mkv_block_slice_t, lace)), // Segment/Cluster/BlockGroup/Slices/TimeSlice/LaceNumber, default 0
    ELEMENT(0xCD,		EBML_TYPE_UINT,		5,	"FrameNumber", ebml_value_parse_uint64,offsetof(struct mkv_block_slice_t, frame)), // Segment/Cluster/BlockGroup/Slices/TimeSlice/FrameNumber, default 0
    ELEMENT(0xCB,		EBML_TYPE_UINT,		5,	"BlockAdditionID", ebml_value_parse_uint64,offsetof(struct mkv_block_slice_t, addition)), // Segment/Cluster/BlockGroup/Slices/TimeSlice/BlockAdditionID, default 0
    ELEMENT(0xCE,		EBML_TYPE_UINT,		5,	"Delay", ebml_value_parse_uint64,offsetof(struct mkv_block_slice_t, delay)), // Segment/Cluster/BlockGroup/Slices/TimeSlice/Delay, default 0
    ELEMENT(0xCF,		EBML_TYPE_UINT,		5,	"SliceDuration", ebml_value_parse_uint64,offsetof(struct mkv_block_slice_t, duration)), // Segment/Cluster/BlockGroup/Slices/TimeSlice/SliceDuration, default 0
    ELEMENT(0xC8,		EBML_TYPE_MASTER,	3,	"ReferenceFrame", NULL, 0), // Segment/Cluster/BlockGroup/ReferenceFrame
    ELEMENT(0xC9,		EBML_TYPE_UINT,		4,	"ReferenceOffset", NULL, 0), // Segment/Cluster/BlockGroup/ReferenceFrame/ReferenceOffset
    ELEMENT(0xCA,		EBML_TYPE_UINT,		4,	"ReferenceTimestamp", NULL, 0), // Segment/Cluster/BlockGroup/ReferenceFrame/ReferenceTimestamp
    ELEMENT(0xAF,		EBML_TYPE_BINARY,	2,	"EncryptedBlock", NULL, 0), // Segment/Cluster/EncryptedBlock [mult]
};

static struct mkv_element_t* mkv_element_find(struct mkv_element_t *elements, size_t count, uint32_t id)
{
	// TODO: to map or tree

	size_t i;
	for (i = 0; i < count; i++)
	{
		if (id == elements[i].id)
			return &elements[i];
	}
	return NULL;
}

static int mkv_reader_open(mkv_reader_t* reader, struct mkv_element_t *elements, size_t count, int level)
{
	int r;
	uint64_t pos;
	struct mkv_element_node_t tree[32]; // max level
	struct mkv_element_node_t *node;
	struct mkv_element_t* e;

	r = 0;
	memset(tree, 0, sizeof(tree));
	while (0 == reader->io.error && 0 == r)
	{
		if (level < 0 || level >= sizeof(tree) / sizeof(tree[0]))
		{
			assert(0); // too many tree levels
			return -1;
		}

		node = &tree[level];
		node->ptr = NULL;
		node->off = mkv_buffer_tell(&reader->io);
		node->id = mkv_buffer_read_id(&reader->io);
		node->size = mkv_buffer_read_size(&reader->io);
		pos = mkv_buffer_tell(&reader->io);
		node->head = pos - node->off;
		if (0 == node->id)
			break;
	
		// https://github.com/ietf-wg-cellar/ebml-specification/blob/master/specification.markdown#unknown-data-size
		// * Only a Master Element is allowed to be of unknown size.
		// The end of an Unknown-Sized Element is determined by whichever comes first:
		// * Any EBML Element that is a valid Parent Element of the Unknown-Sized Element 
		//   according to the EBML Schema, Global Elements excluded.
		// * Any valid EBML Element according to the EBML Schema, Global Elements excluded, that is not a Descendant 
		//   Element of the Unknown-Sized Element but shares a common direct parent, such as a Top-Level Element.
		// * Any EBML Element that is a valid Root Element according to the EBML Schema, Global Elements excluded.
		// * The end of the Parent Element with a known size has been reached.
		// * The end of the EBML Document, either when reaching the end of the file or because a new EBML Header started.

		e = mkv_element_find(elements, count, node->id);
		if (e)
		{
#if defined(MKV_PRINT_ELEMENT) && (defined(DEBUG) || defined(_DEBUG))
			int i;
			for (i = e->level >= 0 ? e->level : level; i > 0; i--)
				printf("\t");
			printf("%s (%" PRId64 ") off: %u\n", e->name, node->size, (unsigned int)node->off);
#endif

			assert(e->level <= level);
			while (e->level < level && -1 != e->level)
			{
				assert(level > 0);
				level--;
				if (tree[level].e)
				{
					assert(-1 == tree[level].size || tree[level].off + tree[level].head + tree[level].size <= node->off);
					assert(tree[level].e);
					tree[level].e = NULL; // pop
				}
			}

			node->e = e;
			node->parent = level > 0 ? &tree[level - 1] : NULL;
			if (EBML_TYPE_MASTER == e->type)
			{
				assert(-1 != e->level);
				memmove(&tree[level], node, sizeof(*node)); // for level skip
				node = &tree[level++];
			}

			assert(level > 0);
			if (e->parse)
			{
				r = e->parse(reader, node);
				assert(r >= 0);
			}
			else if(-1 != node->size)
			{
				mkv_buffer_skip(&reader->io, node->size);
			}
			else
			{
				assert(EBML_TYPE_MASTER == e->type);
			}
		}
		else
		{
			printf("unknown id: 0x%x, size: %" PRId64 "\n", node->id, node->size);
			if (-1 == node->size)
			{
				assert(0);
				return -1;
			}

			mkv_buffer_skip(&reader->io, node->size);
		}
	}

	return r;
}

static int mkv_reader_build(mkv_reader_t* reader)
{
	int i;
	struct mkv_track_t* track;
	struct mkv_segment_info_t* info;

	reader->mkv.duration = 0;
	for (i = 0; i < (int)reader->info_count; i++)
	{
		info = &reader->infos[i];
		reader->mkv.timescale = info->timescale;
		reader->mkv.duration += (info->duration * reader->mkv.timescale / 1000000);
	}

	for (i = 0; i < reader->mkv.track_count; i++)
	{
		track = &reader->mkv.tracks[i];
		track->codecid = mkv_codec_find_id(track->name);
		if (MKV_CODEC_UNKNOWN == track->codecid)
		{
			assert(0);
			return -1;
		}

		// Number of nanoseconds (not scaled via TimestampScale) per frame 
		// ('frame' in the Matroska sense -- one Element put into a (Simple)Block).
		//track->duration = track->duration / reader->mkv.timescale;
	}

#if defined(MKV_LIVE_STREAMING)
	if (0 == reader->mkv.duration && reader->mkv.count > 0)
	{
		reader->mkv.duration = (reader->mkv.samples[reader->mkv.count - 1].dts - reader->mkv.samples[0].dts) * reader->mkv.timescale / 1000000;
	}
#endif

	// seek at cluster
	if (reader->cluster_count > 0)
		mkv_buffer_seek(&reader->io, reader->clusters[0].position);

	return 0;
}

mkv_reader_t* mkv_reader_create(const struct mkv_buffer_t* buffer, void* param)
{
	struct mkv_reader_t* reader;
	reader = (struct mkv_reader_t*)calloc(1, sizeof(*reader));
	if (NULL == reader)
		return NULL;

	memcpy(&reader->io.io, buffer, sizeof(reader->io.io));
	reader->io.param = param;
	reader->ebml.version = 1;
	reader->ebml.read_version = 1;
	reader->ebml.max_id_length = 4;
	reader->ebml.max_size_length = 8;
	reader->ebml.doc_type_version = 1;
	reader->ebml.doc_type_read_version = 1;

	// ignore file read error(for streaming file)
	mkv_reader_open(reader, s_elements, sizeof(s_elements) / sizeof(s_elements[0]), 0);
	if (0 != mkv_reader_build(reader))
	{
		mkv_reader_destroy(reader);
		return NULL;
	}

	return reader;
}

void mkv_reader_destroy(mkv_reader_t* reader)
{
	int i;

	//for (i = 0; i < reader->info_count; i++)
	//	mkv_info_free(reader->infos + i);

	//for (i = 0; i < reader->tag_count; i++)
	//	mkv_tag_free(reader->tags + i);

	//for (i = 0; i < reader->cluster_count; i++)
	//{
	//	FREE(reader->clusters[i].blocks);
	//}

	for (i = 0; i < reader->mkv.track_count; i++)
		mkv_track_free(reader->mkv.tracks + i);
	
	FREE(reader->infos);
	FREE(reader->tags);
	FREE(reader->chapters);
	FREE(reader->clusters);
	FREE(reader->mkv.tracks);
	FREE(reader->mkv.samples);
	FREE(reader->mkv.cue.positions);
	FREE(reader->mkv.rap.raps);
	free(reader);
}

int mkv_reader_getinfo(mkv_reader_t* reader, struct mkv_reader_trackinfo_t* ontrack, void* param)
{
	int i;
	struct mkv_track_t* track;

	for (i = 0; i < reader->mkv.track_count; i++)
	{
		track = &reader->mkv.tracks[i];
		switch (track->name[0])
		{
		case 'V':
			ontrack->onvideo(param, track->id, track->codecid, track->u.video.width, track->u.video.height, track->codec_extra.ptr, track->codec_extra.len);
			break;

		case 'A':
			ontrack->onaudio(param, track->id, track->codecid, track->u.audio.channels, track->u.audio.bits, track->u.audio.sampling, track->codec_extra.ptr, track->codec_extra.len);
			break;

		case 'S':
			ontrack->onsubtitle(param, track->id, track->codecid, track->codec_extra.ptr, track->codec_extra.len);
			break;

		default:
			assert(0);
			break;
		}
	}

	return 0;
}

uint64_t mkv_reader_getduration(mkv_reader_t* reader)
{
	return reader ? reader->mkv.duration : 0;
}

int mkv_reader_read2(mkv_reader_t* reader, mkv_reader_onread2 onread, void* param)
{
	int r;
	void* ptr;
	struct mkv_sample_t* sample;

#if !defined(MKV_LIVE_STREAMING)
	if (reader->offset >= reader->mkv.count)
	{
		reader->offset = 0;
		reader->mkv.count = 0; // clear samples
		reader->mkv.rap.count = 0; // clear rap index

		r = mkv_reader_open(reader, s_clusters, sizeof(s_clusters) / sizeof(s_clusters[0]), 2);
		if (r < 0)
			return r;
	}
#endif

	if (reader->offset >= reader->mkv.count)
		return 0; // eof

	sample = &reader->mkv.samples[reader->offset];
	ptr = onread(param, sample->track, sample->bytes, sample->pts * reader->mkv.timescale / 1000000, sample->dts * reader->mkv.timescale / 1000000, sample->flags);
	if (!ptr)
		return -ENOMEM;

	mkv_buffer_seek(&reader->io, sample->offset);
	mkv_buffer_read(&reader->io, ptr, sample->bytes);
	if (0 != reader->io.error)
		return reader->io.error;

	reader->offset++;
	return 1;
}

static void* mkv_reader_read_helper(void* param, uint32_t track, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	struct mkv_sample_t* sample;
	sample = (struct mkv_sample_t*)param;
	if (sample->bytes < bytes)
		return NULL;

	sample->pts = pts;
	sample->dts = dts;
	sample->flags = flags;
	sample->bytes = (uint32_t)bytes;
	sample->track = track;
	return sample->data;
}

int mkv_reader_read(mkv_reader_t* reader, void* buffer, size_t bytes, mkv_reader_onread onread, void* param)
{
	int r;
	struct mkv_sample_t sample; // temp
	//memset(&sample, 0, sizeof(sample));
	sample.data = buffer;
	sample.bytes = (uint32_t)bytes;
	r = mkv_reader_read2(reader, mkv_reader_read_helper, &sample);
	if (r <= 0)
		return r;

	onread(param, sample.track, buffer, sample.bytes, sample.pts, sample.dts, sample.flags);
	return 1;
}

int mkv_reader_seek(mkv_reader_t* reader, int64_t* timestamp)
{
	uint64_t clock;
	size_t idx, start, end;
#define DIFF(a, b) ((a) > (b) ? ((a) - (b)) : ((b) - (a)))

#if !defined(MKV_LIVE_STREAMING)
	struct mkv_cue_position_t* cue, *prev, *next;

	if (reader->mkv.cue.count < 1)
		return -1;

	idx = start = 0;
	end = reader->mkv.cue.count;
	assert(reader->mkv.cue.count > 0);
	clock = (uint64_t)(*timestamp) * 1000000 / reader->mkv.timescale;

	while (start < end)
	{
		idx = (start + end) / 2;
		cue = &reader->mkv.cue.positions[idx];

		if (cue->timestamp > clock)
			end = idx;
		else if (cue->timestamp < clock)
			start = idx + 1;
		else
			break;
	}

	cue = &reader->mkv.cue.positions[idx];
	prev = &reader->mkv.cue.positions[idx > 0 ? idx - 1 : idx];
	next = &reader->mkv.cue.positions[idx + 1 < reader->mkv.cue.count ? idx + 1 : idx];
	if (DIFF(prev->timestamp, clock) < DIFF(cue->timestamp, clock))
		cue = prev;
	if (DIFF(next->timestamp, clock) < DIFF(cue->timestamp, clock))
		cue = next;

	*timestamp = cue->timestamp * reader->mkv.timescale / 1000000;
	mkv_buffer_seek(&reader->io, cue->cluster);
	reader->offset = reader->mkv.count; // clear
	return 0;
#else
	size_t mid, prev, next;
	struct mkv_sample_t* sample;

	if (reader->mkv.rap.count < 1)
		return -1;
	idx = mid = start = 0;
	end = reader->mkv.rap.count;
	assert(reader->mkv.rap.count > 0);
	clock = (uint64_t)(*timestamp) * 1000000 / reader->mkv.timescale;

	while (start < end)
	{
		mid = (start + end) / 2;
		idx = reader->mkv.rap.raps[mid];

		if (idx < 0 || idx > reader->mkv.count)
		{
			// start from 1
			assert(0);
			return -1;
		}
		idx -= 1;
		sample = &reader->mkv.samples[idx];

		if (sample->dts > clock)
			end = mid;
		else if (sample->dts < clock)
			start = mid + 1;
		else
			break;
	}

	prev = reader->mkv.rap.raps[mid > 0 ? mid - 1 : mid];
	next = reader->mkv.rap.raps[mid + 1 < reader->mkv.rap.count ? mid + 1 : mid];
	if (DIFF(reader->mkv.samples[prev].dts, clock) < DIFF(reader->mkv.samples[idx].dts, clock))
		idx = prev;
	if (DIFF(reader->mkv.samples[next].dts, clock) < DIFF(reader->mkv.samples[idx].dts, clock))
		idx = next;

	*timestamp = reader->mkv.samples[idx].dts * reader->mkv.timescale / 1000000;
	reader->offset = idx;
	return 0;
#endif

#undef DIFF
}
