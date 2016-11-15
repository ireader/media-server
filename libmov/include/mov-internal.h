#ifndef _mov_internal_h_
#define _mov_internal_h_

#include "mov-box.h"
#include "mov-mdhd.h"

#define MOV_TAG(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

#define MOV_MOOV MOV_TAG('m', 'o', 'o', 'v')
#define MOV_ROOT MOV_TAG('r', 'o', 'o', 't')
#define MOV_TRAK MOV_TAG('t', 'r', 'a', 'k')
#define MOV_MDIA MOV_TAG('m', 'd', 'i', 'a')
#define MOV_EDTS MOV_TAG('e', 'd', 't', 's')
#define MOV_MINF MOV_TAG('m', 'i', 'n', 'f')
#define MOV_DINF MOV_TAG('d', 'i', 'n', 'f')
#define MOV_STBL MOV_TAG('s', 't', 'b', 'l')

// stsd: Sample Description Box
#define MOV_AUDIO MOV_TAG('s', 'o', 'u', 'n')
#define MOV_VIDEO MOV_TAG('v', 'i', 'd', 'e')

struct mov_stts_t
{
	uint32_t sample_count;
	uint32_t sample_delta;
};

struct mov_stsc_t
{
	uint32_t first_chunk;
	uint32_t samples_per_chunk;
	uint32_t sample_description_index;
};

struct mov_elst_t
{
	uint64_t segment_duration;
	int64_t media_time;
	int16_t media_rate_integer;
	int16_t media_rate_fraction;
};

struct mov_sample_t
{
	int64_t pts;
	int64_t dts;

	uint64_t offset;
	size_t bytes;

	unsigned int samples_description_index; // stsd
};

struct mov_track_t
{
	struct mov_stsc_t* stsc;
	size_t stsc_count;

	uint64_t* stco;
	size_t stco_count;

	uint32_t* stsz;
	size_t stsz_count;

	struct mov_stts_t* stts;
	size_t stts_count;

	struct mov_stts_t* ctts;
	size_t ctts_count;

	uint32_t* stss;
	size_t stss_count;

	struct mov_elst_t* elst;
	size_t elst_count;

	struct mov_mdhd_t mdhd;

	struct mov_sample_t* samples;
};

struct mov_reader_t
{
	void* fp;
	uint32_t major_brand;
	uint32_t minor_version;
	int header;

	uint32_t handler_type;

	struct mov_track_t* track; // current stream
	struct mov_track_t* tracks;
	size_t track_count;
};

int mov_read_mvhd(struct mov_reader_t* mov, const struct mov_box_t* box);
int mov_read_tkhd(struct mov_reader_t* mov, const struct mov_box_t* box);
int mov_read_mdhd(struct mov_reader_t* mov, const struct mov_box_t* box);
int mov_read_stsd(struct mov_reader_t* mov, const struct mov_box_t* box);
int mov_read_esds(struct mov_reader_t* mov, const struct mov_box_t* box);
int mov_read_stsz(struct mov_reader_t* mov, const struct mov_box_t* box);
int mov_read_stz2(struct mov_reader_t* mov, const struct mov_box_t* box);
int mov_read_stsc(struct mov_reader_t* mov, const struct mov_box_t* box);
int mov_read_stco(struct mov_reader_t* mov, const struct mov_box_t* box);
int mov_read_stts(struct mov_reader_t* mov, const struct mov_box_t* box);
int mov_read_ctts(struct mov_reader_t* mov, const struct mov_box_t* box);
int mov_read_elst(struct mov_reader_t* mov, const struct mov_box_t* box);

#endif /* !_mov_internal_h_ */
