#ifndef _mov_atom_h_
#define _mov_atom_h_

#include <stdint.h>
#include <stddef.h>

#define N_BRAND	8

struct mov_ftyp_t
{
	uint32_t major_brand;
	uint32_t minor_version;

	uint32_t compatible_brands[N_BRAND];
	int brands_count;
};

// A.4 Temporal structure of the media (p148)
// The movie, and each track, has a timescale. 
// This defines a time axis which has a number of ticks per second
struct mov_mvhd_t
{
	// FullBox
	uint32_t version : 8;
	uint32_t flags : 24;

	uint32_t timescale; // time-scale for the entire presentation, the number of time units that pass in one second
	uint64_t duration; // default UINT64_MAX(by timescale)
	uint64_t creation_time; // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t modification_time; // seconds sine midnight, Jan. 1, 1904, UTC

	uint32_t rate;
	uint16_t volume; // fixed point 8.8 number, 1.0 (0x0100) is full volume
	//uint16_t reserved;
	//uint32_t reserved2[2];
	int32_t matrix[9]; // u,v,w
	//int32_t pre_defined[6];
	uint32_t next_track_ID;
};

enum
{
	MOV_TKHD_FLAG_TRACK_ENABLE = 0x01,
	MOV_TKHD_FLAG_TRACK_IN_MOVIE = 0x02,
	MOV_TKHD_FLAG_TRACK_IN_PREVIEW = 0x04,
};

struct mov_tkhd_t
{
	// FullBox
	uint32_t version : 8;
	uint32_t flags : 24; // MOV_TKHD_FLAG_XXX

	uint32_t track_ID; // cannot be zero
	uint64_t creation_time; // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t modification_time; // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t duration; // default UINT64_MAX(by Movie Header Box timescale)
	//uint32_t reserved;

	//uint32_t reserved2[2];
	int16_t layer;
	int16_t alternate_group;
	int16_t volume; // fixed point 8.8 number, 1.0 (0x0100) is full volume
	//uint16_t reserved;	
	int32_t matrix[9]; // u,v,w
	uint32_t width; // fixed-point 16.16 values
	uint32_t height; // fixed-point 16.16 values
};

struct mov_mdhd_t
{
	// FullBox
	uint32_t version : 8;
	uint32_t flags : 24;

	uint32_t timescale; // second
	uint64_t duration; // default UINT64_MAX(by timescale)
	uint64_t creation_time; // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t modification_time; // seconds sine midnight, Jan. 1, 1904, UTC

	uint32_t pad : 1;
	uint32_t language : 15;
	uint32_t pre_defined : 16;
};

struct mov_sample_entry_t
{
    uint16_t data_reference_index; // ref [dref] Data Reference Boxes
    uint8_t object_type_indication; // H.264/AAC MOV_OBJECT_XXX (DecoderConfigDescriptor)
    uint8_t stream_type; // MP4_STREAM_XXX
	uint8_t* extra_data; // H.264 sps/pps
	int extra_data_size;

    union
    {
        struct mov_bitrate_t
        {
            uint32_t bufferSizeDB;
            uint32_t maxBitrate;
            uint32_t avgBitrate;
        } bitrate;

        //struct mov_uri_t
        //{
        //	char uri[256];
        //} uri;

        // visual
        struct mov_visual_sample_t
        {
            uint16_t width;
            uint16_t height;
            uint32_t horizresolution; // 0x00480000 - 72dpi
            uint32_t vertresolution; // 0x00480000 - 72dpi
            uint16_t frame_count; // default 1
            uint16_t depth; // 0x0018

			struct mov_pixel_aspect_ratio_t
			{
				uint32_t h_spacing;
				uint32_t v_spacing;
			} pasp;
        } visual;

        struct mov_audio_sample_t
        {
            uint16_t channelcount; // default 2
            uint16_t samplesize; // default 16
            uint32_t samplerate; // { default samplerate of media } << 16
        } audio;
    } u;
};

struct mov_stsd_t
{
    struct mov_sample_entry_t *current; // current entry, read only
    struct mov_sample_entry_t *entries;
    uint32_t entry_count;
};

struct mov_stts_t
{
	uint32_t sample_count;
	uint32_t sample_delta; // in the time-scale of the media
};

struct mov_stsc_t
{
	uint32_t first_chunk;
	uint32_t samples_per_chunk;
	uint32_t sample_description_index;
};

struct mov_elst_t
{
	uint64_t segment_duration; // by Movie Header Box timescale
	int64_t media_time;
	int16_t media_rate_integer;
	int16_t media_rate_fraction;
};

struct mov_trex_t
{
//	uint32_t track_ID;
	uint32_t default_sample_description_index;
	uint32_t default_sample_duration;
	uint32_t default_sample_size;
	uint32_t default_sample_flags; 
};

struct mov_tfhd_t
{
	uint32_t flags;
//	uint32_t track_ID;
	uint64_t base_data_offset;
	uint32_t sample_description_index;
	uint32_t default_sample_duration;
	uint32_t default_sample_size;
	uint32_t default_sample_flags;
};

#endif /* !_mov_atom_h_ */
