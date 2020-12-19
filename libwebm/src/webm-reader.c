// https://www.matroska.org/technical/elements.html
// https://github.com/cellar-wg/ebml-specification/blob/master/specification.markdown

#include "webm-reader.h"
#include "webm-internal.h"
#include "webm-ioutil.h"
#include "ebml.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

#ifndef offsetof
#define offsetof(s, m)   (size_t)&(((s*)0)->m)
#endif

struct webm_reader_t
{
	struct webm_t webm;
	struct webm_ioutil_t io;

	struct ebml_header_t ebml;

	struct webm_segment_seek_t* seeks;
	size_t seek_count, seek_capacity;

	struct webm_segment_info_t* segments;
	size_t segment_count, segment_capacity;

	struct webm_segment_cluster_t* clusters;
	size_t cluster_count, cluster_capacity;

	struct webm_segment_track_t* tracks;
	size_t track_count, track_capacity;

	struct webm_segment_chapter_t* chapters;
	size_t chapter_count, chapter_capacity;

	struct webm_segment_tag_t* tags;
	size_t tag_count, tag_capacity;

	struct webm_segment_cue_t* cues;
	size_t cue_count, cue_capacity;
};

struct webm_element_node_t;
struct webm_element_t
{
	uint32_t id;
	enum ebml_element_type_e type;
	int level;
	uint32_t parent;

	int (*parse)(struct webm_reader_t* reader, struct webm_element_node_t* node);
	size_t offset;
};

struct webm_element_node_t
{
	const struct webm_element_node_t* parent;

	struct webm_element_t* e;
	void* ptr;
	uint32_t id;
	uint64_t pos;
	int64_t size;
};

enum ebml_element_type_e
{
	ebml_type_unknown,
	ebml_type_int, // Signed Integer Element [0-8]
	ebml_type_uint, // Unsigned Integer Element [0-8]
	ebml_type_float, // Float Element (0/4/8)
	ebml_type_string, // ASCII String Element [0-VINTMAX]
	ebml_type_utf8, // UTF-8 Element [0-VINTMAX]
	ebml_type_date, // Date Element [0-8]
	ebml_type_master, // Master Element [0-VINTMAX]
	ebml_type_binary, // Binary Element [0-VINTMAX]
};

enum {
	EBML_TRACK_VIDEO	= 1,
	EBML_TRACK_AUDIO	= 2,
	EBML_TRACK_COMPLEX	= 3,
	EBML_TRACK_LOGO		= 16,
	EBML_TRACK_SUBTITLE = 17,
	EBML_TRACK_BUTTONS	= 18,
	EBML_TRACK_CONTROL	= 32,
	EBML_TRACK_METADATA = 33,
};

static int ebml_value_parse_bool(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	int* v;
	assert(node->parent);
	v = (int*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(ebml_type_uint == node->e->type);
	assert(0 <= node->size && node->size <= 8);
	*v = webm_buffer_read_uint(&reader->io, (int)node->size) ? 1 : 0;
	return webm_buffer_error(&reader->io);
}

static int ebml_value_parse_int(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	int* v;
	assert(node->parent);
	v = (int*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(ebml_type_int == node->e->type);
	assert(0 <= node->size && node->size <= 8);
	*v = (int)webm_buffer_read_int(&reader->io, (int)node->size);
	return webm_buffer_error(&reader->io);
}

static int ebml_value_parse_uint(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	unsigned int* v;
	assert(node->parent);
	v = (unsigned int*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(ebml_type_uint == node->e->type);
	assert(0 <= node->size && node->size <= 8);
	*v = (unsigned int)webm_buffer_read_uint(&reader->io, (int)node->size);
	return webm_buffer_error(&reader->io);
}

static int ebml_value_parse_int64(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	int64_t* v;
	assert(node->parent);
	v = (int64_t*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(ebml_type_int == node->e->type);
	assert(0 <= node->size && node->size <= 8);
	*v = webm_buffer_read_int(&reader->io, (int)node->size);
	return webm_buffer_error(&reader->io);
}

static int ebml_value_parse_uint64(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	uint64_t* v;
	assert(node->parent);
	v = (uint64_t*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(ebml_type_uint == node->e->type);
	assert(0 <= node->size && node->size <= 8);
	*v = webm_buffer_read_uint(&reader->io, (int)node->size);
	return webm_buffer_error(&reader->io);
}

static int ebml_value_parse_double(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	double* v;
	int64_t r;
	assert(node->parent);
	v = (double*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(ebml_type_float == node->e->type);
	assert(0 == node->size || 4 == node->size || 8 == node->size);
	r = webm_buffer_read_uint(&reader->io, (int)node->size);
	*v = 8 == node->size ? *(double*)&r : *(float*)&r;
	return webm_buffer_error(&reader->io);
}

static int ebml_value_parse_binary(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	void** v;
	assert(node->parent);
	v = (void**)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(ebml_type_string == node->e->type || ebml_type_utf8 == node->e->type || ebml_type_binary == node->e->type);

	*v = malloc(node->size + 1);
	if (!*v)
		return -ENOMEM;
	((char*)*v)[node->size] = 0;
	webm_buffer_read(&reader->io, *v, node->size);
	return webm_buffer_error(&reader->io);
}

static int ebml_value_parse_string(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	return ebml_value_parse_binary(reader, node);
}

static int ebml_value_parse_utf8(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	return ebml_value_parse_string(reader, node);
}

static int ebml_value_parse_date(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	uint64_t* v;
	assert(node->parent);
	v = (uint64_t*)((uint8_t*)node->parent->ptr + node->e->offset);
	assert(ebml_type_date == node->e->type);
	assert(0 == node->size || 8 == node->size);
	webm_buffer_read(&reader->io, v, node->size);
	return webm_buffer_error(&reader->io);
}

static int ebml_default_master_parse(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	node->ptr = node->parent ? node->parent->ptr : NULL; // inherit
	(void)reader;
	return 0; // nothing to do
}

static int ebml_header_parse(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	assert(NULL == node->parent);
	node->ptr = &reader->ebml;
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

static int mkv_segment_seek_parse(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	if (0 != mkv_realloc(&reader->seeks, reader->seek_count, &reader->seek_capacity, sizeof(struct webm_segment_seek_t), 4))
		return -ENOMEM;

	node->ptr = &reader->seeks[reader->seek_count++];
	return 0; // nothing to do
}

static int mkv_segment_info_parse(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	if (0 != mkv_realloc(&reader->segments, reader->segment_count, &reader->segment_capacity, sizeof(struct webm_segment_info_t), 4))
		return -ENOMEM;
	
	node->ptr = &reader->segments[reader->segment_count++];
	return 0; // nothing to do
}

static int mkv_segment_cluster_parse(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	if (0 != mkv_realloc(&reader->clusters, reader->cluster_count, &reader->cluster_capacity, sizeof(struct webm_segment_cluster_t), 4))
		return -ENOMEM;

	node->ptr = &reader->clusters[reader->cluster_count++];
	return 0; // nothing to do
}

static int mkv_segment_cluster_simple_block_parse(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	uint8_t i, j, n;
	uint8_t flags;
	uint64_t pos;
	uint64_t size, total;
	uint64_t track;
	uint16_t timestamp;
	struct webm_segment_cluster_t* cluster;
	struct webm_segment_simple_block_t* block;
	cluster = (struct webm_segment_cluster_t*)node->parent->ptr;
	if (0 != mkv_realloc(&cluster->blocks, cluster->count, &cluster->capacity, sizeof(struct webm_segment_simple_block_t), 64))
		return -ENOMEM;

	block = &cluster->blocks[cluster->count++];
	
	// https://www.matroska.org/technical/basics.html#block-structure
	track = webm_buffer_read_size(&reader->io);
	timestamp = (uint16_t)webm_buffer_read_uint(&reader->io, 2);
	flags = (uint8_t)webm_buffer_read_uint(&reader->io, 1);
	pos = webm_buffer_tell(&reader->io);
	assert(pos - node->pos <= node->size);
	total = node->size - (pos - node->pos);

	if (flags & 0x80) {} // Keyframe
	if (flags & 0x08) {} // Invisible, the codec SHOULD decode this frame but not display it
	if (flags & 0x01) {} // Discardable, the frames of the Block can be discarded during playing if needed

	switch ((flags >> 1) & 0x03) // lacing
	{
	case 1: // Xiph lacing
		n = (uint8_t)webm_buffer_read_uint(&reader->io, 1);
		for (i = 0; i < n; i++)
		{
			size = 0;
			do
			{
				j = webm_buffer_read_uint(&reader->io, 1);
				size += j;
			} while (j == 255);

			if (size > total)
			{
				assert(0);
				return -1;
			}

			total -= size; // remain

			// frame
		}

		// last-frame
		
		break;

	case 2: // fixed-size lacing
		n = (uint8_t)webm_buffer_read_uint(&reader->io, 1);
		size = total / (n + 1);
		assert(size * (n + 1) == total);
		break;

	case 3: // EBML lacing
		n = (uint8_t)webm_buffer_read_uint(&reader->io, 1);
		for (i = 0; i < n; i++)
		{
			size = webm_buffer_read_size(&reader->io);
			if (size > total)
			{
				assert(0);
				return -1;
			}

			total -= size; // remain

			// frame
		}

		// last-frame
		break;

	default: // no lacing
		// frame
		assert(0);
		break;
	}

	return 0; // nothing to do
}

static int mkv_segment_track_parse(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	if (0 != mkv_realloc(&reader->tracks, reader->track_count, &reader->track_capacity, sizeof(struct webm_segment_track_t), 4))
		return -ENOMEM;

	node->ptr = &reader->tracks[reader->track_count++];
	return 0; // nothing to do
}

static int mkv_segment_chapter_parse(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	if (0 != mkv_realloc(&reader->chapters, reader->chapter_count, &reader->chapter_capacity, sizeof(struct webm_segment_chapter_t), 4))
		return -ENOMEM;

	node->ptr = &reader->chapters[reader->chapter_count++];
	return 0; // nothing to do
}

static int mkv_segment_tag_parse(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	if (0 != mkv_realloc(&reader->tags, reader->tag_count, &reader->tag_capacity, sizeof(struct webm_segment_tag_t), 4))
		return -ENOMEM;

	node->ptr = &reader->tags[reader->tag_count++];
	return 0; // nothing to do
}

static int ebml_segment_tag_simple_parse(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	struct webm_segment_tag_t* tag;
	tag = (struct webm_segment_tag_t*)node->parent->ptr;

	if (0 != mkv_realloc(&tag->simples, tag->count, &tag->capacity, sizeof(struct webm_segment_tag_simple_t), 4))
		return -ENOMEM;

	node->ptr = &tag->simples[tag->count++];
	return 0; // nothing to do
}

static int mkv_segment_cue_parse(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	if (0 != mkv_realloc(&reader->cues, reader->cue_count, &reader->cue_capacity, sizeof(struct webm_segment_cue_t), 64))
		return -ENOMEM;

	node->ptr = &reader->cues[reader->cue_count++];
	return 0; // nothing to do
}

static int mkv_segment_cue_position_parse(struct webm_reader_t* reader, struct webm_element_node_t* node)
{
	struct webm_segment_cue_t* cue;
	cue = (struct webm_segment_cue_t*)node->parent->ptr;

	if (0 != mkv_realloc(&cue->positions, cue->count, &cue->capacity, sizeof(struct webm_segment_cue_position_t), 4))
		return -ENOMEM;

	node->ptr = &cue->positions[cue->count++];
	return 0; // nothing to do
}

static struct webm_element_t s_elements[] = {
	// Global
	{ 0xBF,			ebml_type_binary,	-1,	0,			}, // CRC-32 Element, length 4
	{ 0xEC,			ebml_type_binary,	-1,	0,			}, // Void Element

	// EBML
	{ 0x1A45DFA3,	ebml_type_master,	0,	0,			ebml_header_parse, }, // EBML
	{ 0x4286,		ebml_type_uint,		1,	0x1A45DFA3,	ebml_value_parse_uint,	offsetof(struct ebml_header_t, version)}, // EBMLVersion, default 1
	{ 0x42F7,		ebml_type_uint,		1,	0x1A45DFA3,	ebml_value_parse_uint,	offsetof(struct ebml_header_t, read_version)}, // EBMLReadVersion, default 1
	{ 0x42F2,		ebml_type_uint,		1,	0x1A45DFA3,	ebml_value_parse_uint,	offsetof(struct ebml_header_t, max_id_length)}, // EBMLMaxIDLength, default 4
	{ 0x42F3,		ebml_type_uint,		1,	0x1A45DFA3,	ebml_value_parse_uint,	offsetof(struct ebml_header_t, max_size_length)}, // EBMLMaxSizeLength, default 8
	{ 0x4282,		ebml_type_string,	1,	0x1A45DFA3,	ebml_value_parse_string,offsetof(struct ebml_header_t, doc_type)}, // DocType
	{ 0x4287,		ebml_type_uint,		1,	0x1A45DFA3,	ebml_value_parse_uint,	offsetof(struct ebml_header_t, doc_type_version)}, // DocTypeVersion, default 1
	{ 0x4285,		ebml_type_uint,		1,	0x1A45DFA3,	ebml_value_parse_uint,	offsetof(struct ebml_header_t, doc_type_read_version)}, // DocTypeReadVersion, default 1
	{ 0x4281,		ebml_type_master,	1,	0x1A45DFA3,	}, // DocTypeExtension
	{ 0x4283,		ebml_type_string,	2,	0x4281,		}, // DocTypeExtensionName
	{ 0x4284,		ebml_type_uint,		2,	0x4281,		}, // DocTypeExtensionVersion

	// Segment
	{ 0x18538067,	ebml_type_master,	0,	0,			ebml_default_master_parse,	}, // Segment

	// Meta Seek Information
	{ 0x114D9B74,	ebml_type_master,	1,	0x18538067, ebml_default_master_parse,	}, // Segment\SeekHead [mult]
	{ 0x4DBB,		ebml_type_master,	2,	0x114D9B74, mkv_segment_seek_parse,		}, // Segment\SeekHead\Seek [mult]
	{ 0x53AB,		ebml_type_binary,	3,	0x4DBB,		ebml_value_parse_binary,	offsetof(struct webm_segment_seek_t, id),	}, // Segment\SeekHead\Seek\SeekID
	{ 0x53AC,		ebml_type_uint,		3,	0x4DBB,		ebml_value_parse_uint64,	offsetof(struct webm_segment_seek_t, pos),	}, // Segment\SeekHead\Seek\SeekPosition

	// Segment Information
	{ 0x1549A966,	ebml_type_master,	1,	0x18538067, mkv_segment_info_parse,	}, // Segment\Info [mult]
	{ 0x73A4,		ebml_type_binary,	2,	0x1549A966, ebml_value_parse_binary,offsetof(struct webm_segment_info_t, uid),}, // Segment\Info\SegmentUID
	{ 0x7384,		ebml_type_utf8,		2,	0x1549A966, }, // Segment\Info\SegmentFilename
	{ 0x3CB923,		ebml_type_binary,	2,	0x1549A966, }, // Segment\Info\PrevUID
	{ 0x3C83AB,		ebml_type_utf8,		2,	0x1549A966, }, // Segment\Info\PrevFilename
	{ 0x3EB923,		ebml_type_binary,	2,	0x1549A966, }, // Segment\Info\NextUID
	{ 0x3E83BB,		ebml_type_utf8,		2,	0x1549A966, }, // Segment\Info\NextFilename
	{ 0x4444,		ebml_type_binary,	2,	0x1549A966, }, // Segment\Info\SegmentFamily [mult]
	{ 0x6924,		ebml_type_master,	2,	0x1549A966, }, // Segment\Info\ChapterTranslate [mult]
	{ 0x69FC,		ebml_type_uint,		3,	0x6924,		}, // Segment\Info\ChapterTranslate\ChapterTranslateEditionUID [mult]
	{ 0x69BF,		ebml_type_uint,		3,	0x6924,		}, // Segment\Info\ChapterTranslate\ChapterTranslateCodec
	{ 0x69A5,		ebml_type_binary,	3,	0x6924,		}, // Segment\Info\ChapterTranslate\ChapterTranslateID
	{ 0x2AD7B1,		ebml_type_uint,		2,	0x1549A966,	ebml_value_parse_uint,	offsetof(struct webm_segment_info_t, timescale), }, // Segment\Info\TimestampScale, default 1000000
	{ 0x4489,		ebml_type_float,	2,	0x1549A966,	ebml_value_parse_double,offsetof(struct webm_segment_info_t, duration), }, // Segment\Info\Duration
	{ 0x4461,		ebml_type_date,		2,	0x1549A966,	ebml_value_parse_date,offsetof(struct webm_segment_info_t, date), }, // Segment\Info\DateUTC
	{ 0x7BA9,		ebml_type_utf8,		2,	0x1549A966,	}, // Segment\Info\Title
	{ 0x4D80,		ebml_type_utf8,		2,	0x1549A966,	}, // Segment\Info\MuxingApp
	{ 0x5741,		ebml_type_utf8,		2,	0x1549A966,	}, // Segment\Info\WritingApp

	// Cluster
	{ 0x1F43B675,	ebml_type_master,	1,	0x18538067, mkv_segment_cluster_parse, }, // Segment\Cluster [mult]
	{ 0xE7,			ebml_type_uint,		2,	0x1F43B675, ebml_value_parse_uint64,offsetof(struct webm_segment_cluster_t, timestamp),}, // Segment\Cluster\Timestamp
	{ 0x5854,		ebml_type_master,	2,	0x1F43B675, }, // Segment\Cluster\SilentTracks
	{ 0x58D7,		ebml_type_uint,		3,	0x5854,		}, // Segment\Cluster\SilentTracks\SilentTrackNumber [mult]
	{ 0xA7,			ebml_type_uint,		2,	0x1F43B675,	ebml_value_parse_uint64,offsetof(struct webm_segment_cluster_t, position),}, // Segment\Cluster\Position
	{ 0xAB,			ebml_type_uint,		2,	0x1F43B675,	ebml_value_parse_uint64,offsetof(struct webm_segment_cluster_t, prev_size),}, // Segment\Cluster\PrevSize
	{ 0xA3,			ebml_type_binary,	2,	0x1F43B675,	mkv_segment_cluster_simple_block_parse, }, // Segment\Cluster\SimpleBlock [mult]
	{ 0xA0,			ebml_type_master,	2,	0x1F43B675,	}, // Segment\Cluster\BlockGroup [mult]
	{ 0xA1,			ebml_type_binary,	3,	0xA0,		}, // Segment\Cluster\BlockGroup\Block
	{ 0xA2,			ebml_type_binary,	3,	0xA0,		}, // Segment\Cluster\BlockGroup\BlockVirtual
	{ 0x75A1,		ebml_type_master,	3,	0xA0,		}, // Segment\Cluster\BlockGroup\BlockAdditions
	{ 0xA6,			ebml_type_master,	4,	0x75A1,		}, // Segment\Cluster\BlockGroup\BlockAdditions\BlockMore [mult]
	{ 0xEE,			ebml_type_uint,		5,	0xA6,		}, // Segment\Cluster\BlockGroup\BlockAdditions\BlockMore\BlockAddID
	{ 0xA5,			ebml_type_binary,	5,	0xA6,		}, // Segment\Cluster\BlockGroup\BlockAdditions\BlockMore\BlockAdditional
	{ 0x9B,			ebml_type_uint,		3,	0xA0,		}, // Segment\Cluster\BlockGroup\BlockDuration
	{ 0xFA,			ebml_type_uint,		3,	0xA0,		}, // Segment\Cluster\BlockGroup\ReferencePriority, default 0
	{ 0xFB,			ebml_type_int,		3,	0xA0,		}, // Segment\Cluster\BlockGroup\ReferenceBlock [mult]
	{ 0xFD,			ebml_type_int,		3,	0xA0,		}, // Segment\Cluster\BlockGroup\ReferenceVirtual
	{ 0xA4,			ebml_type_binary,	3,	0xA0,		}, // Segment\Cluster\BlockGroup\CodecState
	{ 0x75A2,		ebml_type_int,		3,	0xA0,		}, // Segment\Cluster\BlockGroup\DiscardPadding
	{ 0x8E,			ebml_type_master,	3,	0xA0,		}, // Segment\Cluster\BlockGroup\Slices
	{ 0xE8,			ebml_type_master,	4,	0x8E,		}, // Segment\Cluster\BlockGroup\Slices\TimeSlice [mult]
	{ 0xCC,			ebml_type_uint,		5,	0xE8,		}, // Segment\Cluster\BlockGroup\Slices\TimeSlice\LaceNumber, default 0
	{ 0xCD,			ebml_type_uint,		5,	0xE8,		}, // Segment\Cluster\BlockGroup\Slices\TimeSlice\FrameNumber, default 0
	{ 0xCB,			ebml_type_uint,		5,	0xE8,		}, // Segment\Cluster\BlockGroup\Slices\TimeSlice\BlockAdditionID, default 0
	{ 0xCE,			ebml_type_uint,		5,	0xE8,		}, // Segment\Cluster\BlockGroup\Slices\TimeSlice\Delay, default 0
	{ 0xCF,			ebml_type_uint,		5,	0xE8,		}, // Segment\Cluster\BlockGroup\Slices\TimeSlice\SliceDuration, default 0
	{ 0xC8,			ebml_type_master,	3,	0xA0,		}, // Segment\Cluster\BlockGroup\ReferenceFrame
	{ 0xC9,			ebml_type_uint,		4,	0xC8,		}, // Segment\Cluster\BlockGroup\ReferenceFrame\ReferenceOffset
	{ 0xCA,			ebml_type_uint,		4,	0xC8,		}, // Segment\Cluster\BlockGroup\ReferenceFrame\ReferenceTimestamp
	{ 0xAF,			ebml_type_binary,	2,	0x1F43B675,	}, // Segment\Cluster\EncryptedBlock [mult]

	// Track
	{ 0x1654AE6B,	ebml_type_master,	1,	0x18538067, ebml_default_master_parse, }, // Segment\Tracks
	{ 0xAE,			ebml_type_master,	2,	0x1654AE6B, mkv_segment_track_parse, }, // Segment\Tracks\TrackEntry [mult]
	{ 0xD7,			ebml_type_uint,		3,	0xAE,		ebml_value_parse_uint64,offsetof(struct webm_segment_track_t, number),	}, // Segment\Tracks\TrackEntry\TrackNumber
	{ 0x73C5,		ebml_type_uint,		3,	0xAE,		ebml_value_parse_uint64,offsetof(struct webm_segment_track_t, uid),	}, // Segment\Tracks\TrackEntry\TrackUID
	{ 0x83,			ebml_type_uint,		3,	0xAE,		ebml_value_parse_uint,	offsetof(struct webm_segment_track_t, type),}, // Segment\Tracks\TrackEntry\TrackType, [1-254]
	{ 0xB9,			ebml_type_uint,		3,	0xAE,		ebml_value_parse_bool,	offsetof(struct webm_segment_track_t, enable),}, // Segment\Tracks\TrackEntry\FlagEnabled, default 1
	{ 0x88,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\FlagDefault, default 1
	{ 0x55AA,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\FlagForced, default 0
	{ 0x9C,			ebml_type_uint,		3,	0xAE,		ebml_value_parse_bool,	offsetof(struct webm_segment_track_t, lacing)}, // Segment\Tracks\TrackEntry\FlagLacing, default 1
	{ 0x6DE7,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\MinCache, default 0
	{ 0x6DF8,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\MaxCache
	{ 0x23E383,		ebml_type_uint,		3,	0xAE,		ebml_value_parse_uint64,offsetof(struct webm_segment_track_t, duration)}, // Segment\Tracks\TrackEntry\DefaultDuration
	{ 0x234E7A,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\DefaultDecodedFieldDuration
	{ 0x23314F,		ebml_type_float,	3,	0xAE,		ebml_value_parse_double,offsetof(struct webm_segment_track_t, timescale),}, // Segment\Tracks\TrackEntry\TrackTimestampScale, default 1.0
	{ 0x537F,		ebml_type_int,		3,	0xAE,		ebml_value_parse_int64,	offsetof(struct webm_segment_track_t, offset),}, // Segment\Tracks\TrackEntry\TrackOffset, default 0
	{ 0x55EE,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\MaxBlockAdditionID, default 0
	{ 0x41E4,		ebml_type_master,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\BlockAdditionMapping [mult]
	{ 0x41F0,		ebml_type_uint,		4,	0x41E4,		}, // Segment\Tracks\TrackEntry\BlockAdditionMapping\BlockAddIDValue
	{ 0x41A4,		ebml_type_string,	4,	0x41E4,		}, // Segment\Tracks\TrackEntry\BlockAdditionMapping\BlockAddIDName
	{ 0x41E7,		ebml_type_uint,		4,	0x41E4,		}, // Segment\Tracks\TrackEntry\BlockAdditionMapping\BlockAddIDType
	{ 0x41ED,		ebml_type_binary,	4,	0x41E4,		}, // Segment\Tracks\TrackEntry\BlockAdditionMapping\BlockAddIDExtraData
	{ 0x536E,		ebml_type_utf8,		3,	0xAE,		ebml_value_parse_utf8,	offsetof(struct webm_segment_track_t, name), }, // Segment\Tracks\TrackEntry\Name
	{ 0x22B59C,		ebml_type_string,	3,	0xAE,		ebml_value_parse_string,offsetof(struct webm_segment_track_t, lang), }, // Segment\Tracks\TrackEntry\Language, default eng
	{ 0x22B59D,		ebml_type_string,	3,	0xAE,		ebml_value_parse_string,offsetof(struct webm_segment_track_t, lang), }, // Segment\Tracks\TrackEntry\LanguageIETF
	{ 0x86,			ebml_type_string,	3,	0xAE,		ebml_value_parse_string,offsetof(struct webm_segment_track_t, codec), }, // Segment\Tracks\TrackEntry\CodecID
	{ 0x63A2,		ebml_type_binary,	3,	0xAE,		ebml_value_parse_binary,offsetof(struct webm_segment_track_t, codec_extra) }, // Segment\Tracks\TrackEntry\CodecPrivate
	{ 0x258688,		ebml_type_utf8,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecName
	{ 0x7446,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\AttachmentLink
	{ 0x3A9697,		ebml_type_utf8,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecSettings
	{ 0x3B4040,		ebml_type_string,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecInfoURL [mult]
	{ 0x26B240,		ebml_type_string,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecDownloadURL [mult]
	{ 0xAA,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecDecodeAll
	{ 0x6FAB,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrackOverlay [mult]
	{ 0x56AA,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecDelay
	{ 0x56BB,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\SeekPreRoll
	{ 0x6624,		ebml_type_master,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrackTranslate [mult]
	{ 0x66FC,		ebml_type_uint,		4,	0x6624,		}, // Segment\Tracks\TrackEntry\TrackTranslate\TrackTranslateEditionUID [mult]
	{ 0x66BF,		ebml_type_uint,		4,	0x6624,		}, // Segment\Tracks\TrackEntry\TrackTranslate\TrackTranslateCodec
	{ 0x66A5,		ebml_type_binary,	4,	0x6624,		}, // Segment\Tracks\TrackEntry\TrackTranslate\TrackTranslateTrackID
	{ 0xE0,			ebml_type_master,	3,	0xAE,		ebml_default_master_parse, }, // Segment\Tracks\TrackEntry\Video
	{ 0x9A,			ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\FlagInterlaced, default 0
	{ 0x9D,			ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\FieldOrder, default 2
	{ 0x53B8,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\StereoMode, default 0
	{ 0x53C0,		ebml_type_uint,		4,	0xE0,		ebml_value_parse_bool,	offsetof(struct webm_segment_track_t, u.video.alpha), }, // Segment\Tracks\TrackEntry\Video\AlphaMode, default 0
	{ 0x53B9,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\OldStereoMode
	{ 0xB0,			ebml_type_uint,		4,	0xE0,		ebml_value_parse_uint,	offsetof(struct webm_segment_track_t, u.video.width), }, // Segment\Tracks\TrackEntry\Video\PixelWidth
	{ 0xBA,			ebml_type_uint,		4,	0xE0,		ebml_value_parse_uint,	offsetof(struct webm_segment_track_t, u.video.height), }, // Segment\Tracks\TrackEntry\Video\PixelHeight
	{ 0x54AA,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\PixelCropBottom, default 0
	{ 0x54BB,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\PixelCropTop, default 0
	{ 0x54CC,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\PixelCropLeft, default 0
	{ 0x54DD,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\PixelCropRight, default 0
	{ 0x54B0,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\DisplayWidth
	{ 0x54BA,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\DisplayHeight
	{ 0x54B2,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\DisplayUnit, default 0
	{ 0x54B3,		ebml_type_uint,		4,	0xE0,		ebml_value_parse_uint,	offsetof(struct webm_segment_track_t, u.video.aspect_ratio_type), }, // Segment\Tracks\TrackEntry\Video\AspectRatioType, default 0
	{ 0x2EB524,		ebml_type_binary,	4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\ColourSpace
	{ 0x2FB523,		ebml_type_float,	4,	0xE0,		ebml_value_parse_double,offsetof(struct webm_segment_track_t, u.video.gamma), }, // Segment\Tracks\TrackEntry\Video\GammaValue
	{ 0x2383E3,		ebml_type_float,	4,	0xE0,		ebml_value_parse_double,offsetof(struct webm_segment_track_t, u.video.fps), }, // Segment\Tracks\TrackEntry\Video\FrameRate
	{ 0x55B0,		ebml_type_master,	4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\Colour
	{ 0x55B1,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MatrixCoefficients, default 2
	{ 0x55B2,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\BitsPerChannel, default 0
	{ 0x55B3,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\ChromaSubsamplingHorz
	{ 0x55B4,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\ChromaSubsamplingVert
	{ 0x55B5,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\CbSubsamplingHorz
	{ 0x55B6,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\CbSubsamplingVert
	{ 0x55B7,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\ChromaSitingHorz, default 0
	{ 0x55B8,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\ChromaSitingVert, default 0
	{ 0x55B9,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\Range, default 0
	{ 0x55BA,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\TransferCharacteristics, default 2
	{ 0x55BB,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\Primaries, default 2
	{ 0x55BC,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MaxCLL
	{ 0x55BD,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MaxFALL
	{ 0x55D0,		ebml_type_master,	5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata
	{ 0x55D1,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\PrimaryRChromaticityX
	{ 0x55D2,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\PrimaryRChromaticityY
	{ 0x55D3,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\PrimaryGChromaticityX
	{ 0x55D4,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\PrimaryGChromaticityY
	{ 0x55D5,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\PrimaryBChromaticityX
	{ 0x55D6,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\PrimaryBChromaticityY
	{ 0x55D7,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\WhitePointChromaticityX
	{ 0x55D8,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\WhitePointChromaticityY
	{ 0x55D9,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\LuminanceMax
	{ 0x55DA,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\LuminanceMin
	{ 0x7670,		ebml_type_master,	4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\Projection
	{ 0x7671,		ebml_type_uint,		5,	0x7670,		}, // Segment\Tracks\TrackEntry\Video\ProjectionType
	{ 0x7672,		ebml_type_binary,	5,	0x7670,		}, // Segment\Tracks\TrackEntry\Video\ProjectionPrivate
	{ 0x7673,		ebml_type_float,	5,	0x7670,		}, // Segment\Tracks\TrackEntry\Video\ProjectionPoseYaw
	{ 0x7674,		ebml_type_float,	5,	0x7670,		}, // Segment\Tracks\TrackEntry\Video\ProjectionPosePitch
	{ 0x7675,		ebml_type_float,	5,	0x7670,		}, // Segment\Tracks\TrackEntry\Video\ProjectionPoseRoll
	{ 0xE1,			ebml_type_master,	3,	0xAE,		ebml_default_master_parse, }, // Segment\Tracks\TrackEntry\Audio
	{ 0xB5,			ebml_type_float,	4,	0xE1,		ebml_value_parse_double,offsetof(struct webm_segment_track_t, u.audio.sampling), }, // Segment\Tracks\TrackEntry\Audio\SamplingFrequency, default 8000
	{ 0x78B5,		ebml_type_float,	4,	0xE1,		}, // Segment\Tracks\TrackEntry\Audio\OutputSamplingFrequency
	{ 0x9F,			ebml_type_uint,		4,	0xE1,		ebml_value_parse_uint,	offsetof(struct webm_segment_track_t, u.audio.channels), }, // Segment\Tracks\TrackEntry\Audio\Channels, default 1
	{ 0x7D7B,		ebml_type_binary,	4,	0xE1,		ebml_value_parse_uint, }, // Segment\Tracks\TrackEntry\Audio\ChannelPositions
	{ 0x6264,		ebml_type_uint,		4,	0xE1,		ebml_value_parse_uint,	offsetof(struct webm_segment_track_t, u.audio.bits), }, // Segment\Tracks\TrackEntry\Audio\BitDepth
	{ 0xE2,			ebml_type_master,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrackOperation
	{ 0xE3,			ebml_type_master,	4,	0xE2,		}, // Segment\Tracks\TrackEntry\TrackOperation\TrackCombinePlanes
	{ 0xE4,			ebml_type_master,	5,	0xE3,		}, // Segment\Tracks\TrackEntry\TrackOperation\TrackCombinePlanes\TrackPlane [mult]
	{ 0xE5,			ebml_type_uint,		6,	0xE4,		}, // Segment\Tracks\TrackEntry\TrackOperation\TrackCombinePlanes\TrackPlane\TrackPlaneUID
	{ 0xE6,			ebml_type_uint,		6,	0xE4,		}, // Segment\Tracks\TrackEntry\TrackOperation\TrackCombinePlanes\TrackPlane\TrackPlaneType
	{ 0xE9,			ebml_type_master,	4,	0xE2,		}, // Segment\Tracks\TrackEntry\TrackOperation\TrackJoinBlocks
	{ 0xED,			ebml_type_uint,		5,	0xE9,		}, // Segment\Tracks\TrackEntry\TrackOperation\TrackJoinBlocks\TrackJoinUID
	{ 0xC0,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrickTrackUID // 	DivX trick track extensions
	{ 0xC1,			ebml_type_binary,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrickTrackSegmentUID // 	DivX trick track extensions
	{ 0xC6,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrickTrackFlag // 	DivX trick track extensions
	{ 0xC7,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrickMasterTrackUID // 	DivX trick track extensions
	{ 0xC4,			ebml_type_binary,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrickMasterTrackSegmentUID // 	DivX trick track extensions
	{ 0x6D80,		ebml_type_master,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\ContentEncodings
	{ 0x6240,		ebml_type_master,	4,	0x6D80,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding [mult]
	{ 0x5031,		ebml_type_uint,		5,	0x6240,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncodingOrder, default 0
	{ 0x5032,		ebml_type_uint,		5,	0x6240,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncodingScope, default 1
	{ 0x5033,		ebml_type_uint,		5,	0x6240,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncodingType, default 0
	{ 0x5034,		ebml_type_master,	5,	0x6240,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentCompression
	{ 0x4254,		ebml_type_uint,		6,	0x5034,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentCompression\ContentCompAlgo, default 0
	{ 0x4255,		ebml_type_binary,	6,	0x5034,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentCompression\ContentCompSettings
	{ 0x5035,		ebml_type_master,	5,	0x6240,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption
	{ 0x47E1,		ebml_type_uint,		6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentEncAlgo, default 0
	{ 0x47E2,		ebml_type_binary,	6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentEncKeyID
	{ 0x47E7,		ebml_type_master,	6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentEncAESSettings
	{ 0x47E8,		ebml_type_uint,		7,	0x47E7,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentEncAESSettings\AESSettingsCipherMode
	{ 0x47E3,		ebml_type_binary,	6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentSignature
	{ 0x47E4,		ebml_type_binary,	6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentSigKeyID
	{ 0x47E5,		ebml_type_uint,		6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentSigAlgo
	{ 0x47E6,		ebml_type_uint,		6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentSigHashAlgo

	// Cueing Data
	{ 0x1C53BB6B,	ebml_type_master,	1,	0x18538067, ebml_default_master_parse, }, // Segment\Cues
	{ 0xBB,			ebml_type_master,	2,	0x1C53BB6B, mkv_segment_cue_parse, }, // Segment\Cues\CuePoint [mult]
	{ 0xB3,			ebml_type_uint,		3,	0xBB,		ebml_value_parse_uint64,offsetof(struct webm_segment_cue_t, time), }, // Segment\Cues\CuePoint\CueTime
	{ 0xB7,			ebml_type_master,	3,	0xBB,		mkv_segment_cue_position_parse }, // Segment\Cues\CuePoint\CueTrackPositions [mult]
	{ 0xF7,			ebml_type_uint,		4,	0xB7,		ebml_value_parse_uint64,offsetof(struct webm_segment_cue_position_t, track), }, // Segment\Cues\CuePoint\CueTrackPositions\CueTrack
	{ 0xF1,			ebml_type_uint,		4,	0xB7,		ebml_value_parse_uint64,offsetof(struct webm_segment_cue_position_t, cluster) }, // Segment\Cues\CuePoint\CueTrackPositions\CueClusterPosition
	{ 0xF0,			ebml_type_uint,		4,	0xB7,		ebml_value_parse_uint64,offsetof(struct webm_segment_cue_position_t, relative) }, // Segment\Cues\CuePoint\CueTrackPositions\CueRelativePosition
	{ 0xB2,			ebml_type_uint,		4,	0xB7,		ebml_value_parse_uint64,offsetof(struct webm_segment_cue_position_t, duration) }, // Segment\Cues\CuePoint\CueTrackPositions\CueDuration
	{ 0x5378,		ebml_type_uint,		4,	0xB7,		ebml_value_parse_uint64,offsetof(struct webm_segment_cue_position_t, block) }, // Segment\Cues\CuePoint\CueTrackPositions\CueBlockNumber, default 1
	{ 0xEA,			ebml_type_uint,		4,	0xB7, }, // Segment\Cues\CuePoint\CueTrackPositions\CueCodecState, default 0
	{ 0xDB,			ebml_type_master,	4,	0xB7, }, // Segment\Cues\CuePoint\CueTrackPositions\CueReference [mult]
	{ 0x96,			ebml_type_uint,		5,	0xDB, }, // Segment\Cues\CuePoint\CueTrackPositions\CueReference\CueRefTime
	{ 0x97,			ebml_type_uint,		5,	0xDB, }, // Segment\Cues\CuePoint\CueTrackPositions\CueReference\CueRefCluster
	{ 0x535F,		ebml_type_uint,		5,	0xDB, }, // Segment\Cues\CuePoint\CueTrackPositions\CueReference\CueRefNumber, default 1
	{ 0xEB,			ebml_type_uint,		5,	0xDB, }, // Segment\Cues\CuePoint\CueTrackPositions\CueReference\CueRefCodecState, default 0

	// Attachment
	{ 0x1941A469,	ebml_type_master,	1,	0x18538067, }, // Segment\Attachments
	{ 0x61A7,		ebml_type_master,	2,	0x1941A469, }, // Segment\Attachments\AttachedFile [mult]
	{ 0x467E,		ebml_type_utf8,		3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileDescription
	{ 0x466E,		ebml_type_utf8,		3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileName
	{ 0x4660,		ebml_type_string,	3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileMimeType
	{ 0x465C,		ebml_type_binary,	3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileData
	{ 0x46AE,		ebml_type_uint,		3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileUID
	{ 0x4675,		ebml_type_binary,	3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileReferral
	{ 0x4661,		ebml_type_uint,		3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileUsedStartTime
	{ 0x4662,		ebml_type_uint,		3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileUsedEndTime

	// Chapters
	{ 0x1043A770,	ebml_type_master,	1,	0x18538067, ebml_default_master_parse, }, // Segment\Chapters
	{ 0x45B9,		ebml_type_master,	2,	0x1043A770, mkv_segment_chapter_parse }, // Segment\Chapters\EditionEntry [mult]
	{ 0x45BC,		ebml_type_uint,		3,	0x45B9,		ebml_value_parse_uint,	offsetof(struct webm_segment_chapter_t, uid), }, // Segment\Chapters\EditionEntry\EditionUID
	{ 0x45BD,		ebml_type_uint,		3,	0x45B9,		ebml_value_parse_uint,	offsetof(struct webm_segment_chapter_t, flag_hidden), }, // Segment\Chapters\EditionEntry\EditionFlagHidden, default 0
	{ 0x45DB,		ebml_type_uint,		3,	0x45B9,		ebml_value_parse_uint,	offsetof(struct webm_segment_chapter_t, flag_default), }, // Segment\Chapters\EditionEntry\EditionFlagDefault, default 0
	{ 0x45DD,		ebml_type_uint,		3,	0x45B9,		ebml_value_parse_uint,	offsetof(struct webm_segment_chapter_t, flag_ordered), }, // Segment\Chapters\EditionEntry\EditionFlagOrdered, default 0
	{ 0xB6,			ebml_type_master,	3,	0x45B9,		}, // Segment\Chapters\EditionEntry\ChapterAtom [mult]
	{ 0x73C4,		ebml_type_uint,		4,	0xB6,		}, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterUID
	{ 0x5654,		ebml_type_utf8,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterStringUID
	{ 0x91,			ebml_type_uint,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterTimeStart
	{ 0x92,			ebml_type_uint,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterTimeEnd
	{ 0x98,			ebml_type_uint,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterFlagHidden, default 0
	{ 0x4598,		ebml_type_uint,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterFlagEnabled, default 1
	{ 0x6E67,		ebml_type_binary,	4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterSegmentUID
	{ 0x6EBC,		ebml_type_uint,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterSegmentEditionUID
	{ 0x63C3,		ebml_type_uint,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterPhysicalEquiv
	{ 0x8F,			ebml_type_master,	4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterTrack
	{ 0x89,			ebml_type_uint,		5,	0x8F, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterTrack\ChapterTrackUID [mult]
	{ 0x80,			ebml_type_master,	4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterDisplay [mult]
	{ 0x85,			ebml_type_utf8,		5,	0x80, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterDisplay\ChapString
	{ 0x437C,		ebml_type_string,	5,	0x80, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterDisplay\ChapLanguage [mult], default eng
	{ 0x437D,		ebml_type_string,	5,	0x80, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterDisplay\ChapLanguageIETF [mult]
	{ 0x437E,		ebml_type_string,	5,	0x80, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterDisplay\ChapCountry [mult]
	{ 0x6944,		ebml_type_master,	4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapProcess [mult]
	{ 0x6955,		ebml_type_uint,		5,	0x6944, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapProcess\ChapProcessCodecID
	{ 0x450D,		ebml_type_binary,	5,	0x6944, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapProcess\ChapProcessPrivate
	{ 0x6911,		ebml_type_master,	5,	0x6944, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapProcess\ChapProcessCommand [mult]
	{ 0x6922,		ebml_type_uint,		6,	0x6911, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapProcess\ChapProcessComman\ChapProcessTime
	{ 0x6933,		ebml_type_binary,	6,	0x6911, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapProcess\ChapProcessComman\ChapProcessData

	// Tagging
	{ 0x1254C367,	ebml_type_master,	1,	0x18538067, ebml_default_master_parse, }, // Segment\Tags [mult]
	{ 0x7373,		ebml_type_master,	2,	0x1254C367, mkv_segment_tag_parse }, // Segment\Tags\Tag [mult]
	{ 0x63C0,		ebml_type_master,	3,	0x7373, }, // Segment\Tags\Tag\Targets
	{ 0x68CA,		ebml_type_uint,		4,	0x63C0, }, // Segment\Tags\Tag\Targets\TargetTypeValue, default 50
	{ 0x63CA,		ebml_type_string,	4,	0x63C0, }, // Segment\Tags\Tag\Targets\TargetType
	{ 0x63C5,		ebml_type_uint,		4,	0x63C0, }, // Segment\Tags\Tag\Targets\TagTrackUID [mult]
	{ 0x63C9,		ebml_type_uint,		4,	0x63C0, }, // Segment\Tags\Tag\Targets\TagEditionUID [mult]
	{ 0x63C4,		ebml_type_uint,		4,	0x63C0, }, // Segment\Tags\Tag\Targets\TagChapterUID [mult]
	{ 0x63C6,		ebml_type_uint,		4,	0x63C0, }, // Segment\Tags\Tag\Targets\TagAttachmentUID [mult]
	{ 0x67C8,		ebml_type_master,	3,	0x7373, ebml_segment_tag_simple_parse, }, // Segment\Tags\Tag\SimpleTag [mult]
	{ 0x45A3,		ebml_type_utf8,		4,	0x67C8, ebml_value_parse_utf8,	offsetof(struct webm_segment_tag_simple_t, name), }, // Segment\Tags\Tag\SimpleTag\TagName
	{ 0x447A,		ebml_type_string,	4,	0x67C8, ebml_value_parse_string,offsetof(struct webm_segment_tag_simple_t, lang), }, // Segment\Tags\Tag\SimpleTag\TagLanguage, default und
	{ 0x447B,		ebml_type_string,	4,	0x67C8, ebml_value_parse_string,offsetof(struct webm_segment_tag_simple_t, lang), }, // Segment\Tags\Tag\SimpleTag\TagLanguageIETF
	{ 0x4484,		ebml_type_uint,		4,	0x67C8, }, // Segment\Tags\Tag\SimpleTag\TagDefault, default 1
	{ 0x4487,		ebml_type_utf8,		4,	0x67C8, ebml_value_parse_utf8,	offsetof(struct webm_segment_tag_simple_t, string), }, // Segment\Tags\Tag\SimpleTag\TagString
	{ 0x4485,		ebml_type_binary,	4,	0x67C8, }, // Segment\Tags\Tag\SimpleTag\TagBinary
};

static struct webm_element_t* webm_element_find(uint32_t id)
{
	// TODO: to map or tree

	int i;
	for (i = 0; i < sizeof(s_elements) / sizeof(s_elements[0]); i++)
	{
		if (id == s_elements[i].id)
			return &s_elements[i];
	}
	return NULL;
}

static int webm_reader_open(webm_reader_t* reader)
{
	int r;
	int level;
	uint64_t pos;
	struct webm_element_node_t tree[32]; // max level
	struct webm_element_node_t *node;
	struct webm_element_t* e;

	r = 0;
	level = 0;
	memset(tree, 0, sizeof(tree));
	while (0 == reader->io.error && 0 == r)
	{
		if (level < 0 || level >= sizeof(tree) / sizeof(tree[0]))
		{
			assert(0); // too many tree levels
			return -1;
		}

		pos = webm_buffer_tell(&reader->io);
		node = &tree[level];
		node->ptr = NULL;
		node->id = webm_buffer_read_id(&reader->io);
		node->size = webm_buffer_read_size(&reader->io);
		node->pos = webm_buffer_tell(&reader->io);

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

		e = webm_element_find(node->id);
		if (e)
		{	
			assert(e->level <= level);
			while (e->level < level && -1 != e->level)
			{
				assert(level > 0);
				level--;
				assert(-1 == tree[level].size || tree[level].pos + tree[level].size == pos);
				assert(tree[level].e);
				tree[level].e = NULL; // pop
			}

			node->e = e;
			node->parent = level > 0 ? &tree[level - 1] : NULL;
			if (ebml_type_master == e->type)
			{
				assert(-1 != e->level);
				memmove(&tree[level], node, sizeof(*node)); // for level skip
				node = &tree[level++];
			}

			assert(level > 0);
			if (e->parse)
			{
				r = e->parse(reader, node);
				assert(0 == r);
			}
			else if(-1 != node->size)
			{
				webm_buffer_skip(&reader->io, node->size);
			}
			else
			{
				assert(ebml_type_master == e->type);
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

			webm_buffer_skip(&reader->io, node->size);
		}
	}

	return r;
}

webm_reader_t* webm_reader_create(const struct webm_buffer_t* buffer, void* param)
{
	struct webm_reader_t* reader;
	reader = (struct webm_reader_t*)calloc(1, sizeof(*reader));
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

	if (0 != webm_reader_open(reader))
	{
		webm_reader_destroy(reader);
		return NULL;
	}

	return reader;
}

void webm_reader_destroy(webm_reader_t* reader)
{
	int i;
	//for (i = 0; i < reader->webm.track_count; i++)
	//	webm_free_track(reader->webm.tracks + i);
	if (reader->webm.tracks)
		free(reader->webm.tracks);
	free(reader);
}

int webm_reader_getinfo(webm_reader_t* reader, struct webm_reader_trackinfo_t* ontrack, void* param)
{
	return -1;
}

uint64_t webm_reader_getduration(webm_reader_t* reader)
{
	return 0;
}

int webm_reader_read(webm_reader_t* reader, void* buffer, size_t bytes, webm_reader_onread onread, void* param)
{
	return -1;
}

int webm_reader_seek(webm_reader_t* reader, int64_t* timestamp)
{
	return -1;
}
