#ifndef _mov_internal_h_
#define _mov_internal_h_

#include "mov-box.h"
#include "mov-atom.h"
#include "mov-format.h"

#define MOV_TAG(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

#define MOV_MOOV MOV_TAG('m', 'o', 'o', 'v')
#define MOV_ROOT MOV_TAG('r', 'o', 'o', 't')
#define MOV_TRAK MOV_TAG('t', 'r', 'a', 'k')
#define MOV_MDIA MOV_TAG('m', 'd', 'i', 'a')
#define MOV_EDTS MOV_TAG('e', 'd', 't', 's')
#define MOV_MINF MOV_TAG('m', 'i', 'n', 'f')
#define MOV_DINF MOV_TAG('d', 'i', 'n', 'f')
#define MOV_STBL MOV_TAG('s', 't', 'b', 'l')

#define MOV_VIDEO MOV_TAG('v', 'i', 'd', 'e')
#define MOV_AUDIO MOV_TAG('s', 'o', 'u', 'n')
#define MOV_HINT  MOV_TAG('h', 'i', 'n', 't')
#define MOV_META  MOV_TAG('m', 'e', 't', 'a')

#define MOV_H264 MOV_TAG('a', 'v', 'c', '1') // H.264
#define MOV_HEVC MOV_TAG('h', 'e', 'v', '1') // H.265
#define MOV_MP4V MOV_TAG('m', 'p', '4', 'v') // MPEG-4 Video
#define MOV_MP4A MOV_TAG('m', 'p', '4', 'a') // AAC

// ISO/IEC 14496-1:2010(E) 7.2.6.6 DecoderConfigDescriptor
// Table 6 - streamType Values (p51)
enum
{
	MP4_STREAM_ODS		= 0x01, /* ObjectDescriptorStream */
	MP4_STREAM_CRS		= 0x02, /* ClockReferenceStream */
	MP4_STREAM_SDS		= 0x03, /* SceneDescriptionStream */
	MP4_STREAM_VISUAL	= 0x04, /* VisualStream */
	MP4_STREAM_AUDIO	= 0x05, /* AudioStream */
	MP4_STREAM_MP7		= 0x06, /* MPEG7Stream */
	MP4_STREAM_IPMP		= 0x07, /* IPMPStream */
	MP4_STREAM_OCIS		= 0x08, /* ObjectContentInfoStream */
	MP4_STREAM_MPEGJ	= 0x09, /* MPEGJStream */
	MP4_STREAM_IS		= 0x0A, /* Interaction Stream */
	MP4_STREAM_IPMPTOOL = 0x0B, /* IPMPToolStream */
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
	MOV_BRAND_MP41 = MOV_TAG('m', 'p', '4', '1'), // ISO/IEC 14496-1:2001 MP4 File Format v1
	MOV_BRAND_MP42 = MOV_TAG('m', 'p', '4', '2'), // ISO/IEC 14496-14:2003 MP4 File Format v2
	MOV_BRAND_MOV  = MOV_TAG('q', 't', ' ', ' '), // Apple Quick-Time File Format
};

struct mov_stbl_t
{
	struct mov_stsc_t* stsc;
	size_t stsc_count;

	uint64_t* stco;
	size_t stco_count;

	struct mov_stts_t* stts;
	size_t stts_count;

	struct mov_stts_t* ctts;
	size_t ctts_count;

	uint32_t* stss;
	size_t stss_count;
};

struct mov_sample_t
{
	int flags; // MOV_AV_FLAG_KEYFREAME
	int64_t pts;
	int64_t dts;

	uint64_t offset; // is a 32 or 64 bit integer that gives the offset of the start of a chunk into its containing media file.
	size_t bytes;

	struct mov_stsd_t* stsd;

	// only for write
	union
	{
		struct mov_stsc_t stsc;
		struct
		{
			uint32_t count;
			uint32_t duration;
		} stts;
	} u;
};

struct mov_track_t
{
	uint32_t tag; // MOV_H264/MOV_MP4A
	uint32_t handler_type; // MOV_VIDEO/MOV_AUDIO
	uint8_t* extra_data; // H.264 sps/pps
	size_t extra_data_size;

	struct mov_tkhd_t tkhd;
	struct mov_mdhd_t mdhd;

	struct mov_stsd_t* stsd;
	size_t stsd_count;

	struct mov_elst_t* elst;
	size_t elst_count;

	struct mov_stbl_t stbl;

	struct mov_sample_t* samples;
	size_t sample_count;
	size_t sample_offset; // sample_capacity

	int64_t start_dts;
	int64_t start_cts;
	int64_t end_pts;
	uint64_t offset; // write offset
};

struct mov_t
{
	void* fp;
	
	struct mov_ftyp_t ftyp;
	struct mov_mvhd_t mvhd;

	int header;

	struct mov_track_t* track; // current stream
	struct mov_track_t* tracks;
	size_t track_count;
};

int mov_reader_box(struct mov_t* mov, const struct mov_box_t* parent);

int mov_read_ftyp(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_mvhd(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_tkhd(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_hdlr(struct mov_t* mov, const struct mov_box_t* box);
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
int mov_read_avcc(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_hvcc(struct mov_t* mov, const struct mov_box_t* box);

size_t mov_write_ftyp(const struct mov_t* mov);
size_t mov_write_mvhd(const struct mov_t* mov);
size_t mov_write_mdhd(const struct mov_t* mov);
size_t mov_write_tkhd(const struct mov_t* mov);
size_t mov_write_hdlr(const struct mov_t* mov);
size_t mov_write_minf(const struct mov_t* mov);
size_t mov_write_dinf(const struct mov_t* mov);
size_t mov_write_elst(const struct mov_t* mov);
size_t mov_write_stsd(const struct mov_t* mov);
size_t mov_write_stts(const struct mov_t* mov, uint32_t count);
size_t mov_write_ctts(const struct mov_t* mov, uint32_t count);
size_t mov_write_stco(const struct mov_t* mov, uint32_t count);
size_t mov_write_stss(const struct mov_t* mov);
size_t mov_write_stsc(const struct mov_t* mov);
size_t mov_write_stsz(const struct mov_t* mov);
size_t mov_write_stbl(const struct mov_t* mov);
size_t mov_write_esds(const struct mov_t* mov);
size_t mov_write_avcc(const struct mov_t* mov);
size_t mov_write_hvcc(const struct mov_t* mov);

void mov_write_size(void* fp, uint64_t offset, size_t size);

size_t mov_stco_size(const struct mov_track_t* track, uint64_t offset);

uint8_t mov_tag_to_object(uint32_t tag);
uint32_t mov_object_to_tag(uint8_t object);

#endif /* !_mov_internal_h_ */
