#ifndef _mov_internal_h_
#define _mov_internal_h_

#include "mov-box.h"
#include "mov-tkhd.h"
#include "mov-mvhd.h"
#include "mov-mdhd.h"

#define N_BRAND	8

#define MOV_TAG(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

#define MOV_MOOV MOV_TAG('m', 'o', 'o', 'v')
#define MOV_ROOT MOV_TAG('r', 'o', 'o', 't')
#define MOV_TRAK MOV_TAG('t', 'r', 'a', 'k')
#define MOV_MDIA MOV_TAG('m', 'd', 'i', 'a')
#define MOV_EDTS MOV_TAG('e', 'd', 't', 's')
#define MOV_MINF MOV_TAG('m', 'i', 'n', 'f')
#define MOV_DINF MOV_TAG('d', 'i', 'n', 'f')
#define MOV_STBL MOV_TAG('s', 't', 'b', 'l')

enum AVCodecID
{
	AV_VIDEO_H264 = 1,
	AV_VIDEO_H265,
	AV_VIDEO_VP8,
	AV_VIDEO_VP9,
	AV_VIDEO_MPEG2,
	AV_VIDEO_MPEG4,
	
	AV_AUDIO_AAC = 10001,
	AV_AUDIO_MP3,
	AV_AUDIO_OPUS,
	AV_AUDIO_G726,
	AV_AUDIO_G729,
};

enum AVStreamType
{
	AVSTREAM_VIDEO = 1,
	AVSTREAM_AUDIO = 2,
	AVSTREAM_TEXT  = 3,
};

enum
{
	MOV_BRAND_ISOM = MOV_TAG('i', 's', 'o', 'm'),
	MOV_BRAND_AVC1 = MOV_TAG('a', 'v', 'c', '1'),
	MOV_BRAND_ISO2 = MOV_TAG('i', 's', 'o', '2'),
	MOV_BRAND_MP71 = MOV_TAG('m', 'p', '7', '1'),
	MOV_BRAND_ISO3 = MOV_TAG('i', 's', 'o', '3'),
	MOV_BRAND_ISO4 = MOV_TAG('i', 's', 'o', '4'),
	MOV_BRAND_ISO5 = MOV_TAG('i', 's', 'o', '5'),
	MOV_BRAND_ISO6 = MOV_TAG('i', 's', 'o', '6'),
	MOV_BRAND_MP41 = MOV_TAG('m', 'p', '4', '1'), // MP4 File Format v1
	MOV_BRAND_MP42 = MOV_TAG('m', 'p', '4', '2'), // MP4 File Format v2
	MOV_BRAND_MOV  = MOV_TAG('q', 't', ' ', ' '), // Apple Quick-Time File Format
};

struct mov_ftyp_t
{
	uint32_t major_brand;
	uint32_t minor_version;

	uint32_t compatible_brands[N_BRAND];
	size_t brands_count;
};

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

	uint32_t sample_description_index;

	int flags;

	// only for write
	union
	{
		struct mov_stsc_t chunk;
		struct 
		{
			uint32_t count;
			int32_t duration;
		} timestamp;
	} u;
};

struct mov_track_t
{
	uint32_t id;
	uint32_t codec_id; // H.264/AAC fourcc
	uint32_t stream_type; // AVSTREAM_VIDEO/AVSTREAM_AUDIO

	union
	{
		struct  
		{
			uint16_t width;
			uint16_t height;
		} video;

		struct
		{
			uint16_t sample_rate;
			uint16_t channels;
			uint16_t bits_per_sample;
		} audio;
	} av;

	uint8_t* extra_data; // H.264 sps/pps
	size_t extra_data_size;

	struct mov_tkhd_t tkhd;
	struct mov_mvhd_t mvhd;

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
	//size_t sample_count; // same as stsz_count
	size_t chunk_count;
};

struct mov_t
{
	void* fp;
	
	struct mov_ftyp_t ftyp;

	int header;

	uint32_t handler_type;

	struct mov_track_t* track; // current stream
	struct mov_track_t* tracks;
	size_t track_count;
};

int mov_read_ftyp(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_mvhd(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_tkhd(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_mdhd(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_vmhd(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_smhd(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_esds(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_elst(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_stsd(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_stsz(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_stz2(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_stsc(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_stco(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_stts(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_ctts(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_stss(struct mov_t* mov, const struct mov_box_t* box);

size_t mov_write_ftyp(const struct mov_t* mov);
size_t mov_write_mvhd(const struct mov_t* mov);
size_t mov_write_mdhd(const struct mov_t* mov);
size_t mov_write_tkhd(const struct mov_t* mov);
size_t mov_write_hdlr(const struct mov_t* mov);
size_t mov_write_minf(const struct mov_t* mov);
size_t mov_write_elst(const struct mov_t* mov);
size_t mov_write_stsd(const struct mov_t* mov);
size_t mov_write_stts(const struct mov_t* mov);
size_t mov_write_ctts(const struct mov_t* mov);
size_t mov_write_stsc(const struct mov_t* mov);
size_t mov_write_stsz(const struct mov_t* mov);
size_t mov_write_stco(const struct mov_t* mov);

void mov_write_size(void* fp, uint64_t offset, size_t size);

#endif /* !_mov_internal_h_ */
