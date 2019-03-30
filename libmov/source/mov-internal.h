#ifndef _mov_internal_h_
#define _mov_internal_h_

#include "mov-box.h"
#include "mov-atom.h"
#include "mov-format.h"
#include "mov-buffer.h"
#include "mov-ioutil.h"

#define MOV_TAG(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

#define MOV_MOOV MOV_TAG('m', 'o', 'o', 'v')
#define MOV_ROOT MOV_TAG('r', 'o', 'o', 't')
#define MOV_TRAK MOV_TAG('t', 'r', 'a', 'k')
#define MOV_MDIA MOV_TAG('m', 'd', 'i', 'a')
#define MOV_EDTS MOV_TAG('e', 'd', 't', 's')
#define MOV_MINF MOV_TAG('m', 'i', 'n', 'f')
#define MOV_DINF MOV_TAG('d', 'i', 'n', 'f')
#define MOV_STBL MOV_TAG('s', 't', 'b', 'l')
#define MOV_MVEX MOV_TAG('m', 'v', 'e', 'x')
#define MOV_MOOF MOV_TAG('m', 'o', 'o', 'f')
#define MOV_TRAF MOV_TAG('t', 'r', 'a', 'f')
#define MOV_MFRA MOV_TAG('m', 'f', 'r', 'a')

#define MOV_VIDEO	MOV_TAG('v', 'i', 'd', 'e') // ISO/IEC 14496-12:2015(E) 12.1 Video media (p169)
#define MOV_AUDIO	MOV_TAG('s', 'o', 'u', 'n') // ISO/IEC 14496-12:2015(E) 12.2 Audio media (p173)
#define MOV_META	MOV_TAG('m', 'e', 't', 'a') // ISO/IEC 14496-12:2015(E) 12.3 Metadata media (p181)
#define MOV_HINT	MOV_TAG('h', 'i', 'n', 't') // ISO/IEC 14496-12:2015(E) 12.4 Hint media (p183)
#define MOV_TEXT	MOV_TAG('t', 'e', 'x', 't') // ISO/IEC 14496-12:2015(E) 12.5 Text media (p184)
#define MOV_SUBT	MOV_TAG('s', 'u', 'b', 't') // ISO/IEC 14496-12:2015(E) 12.6 Subtitle media (p185)
#define MOV_FONT	MOV_TAG('f', 'd', 's', 'm') // ISO/IEC 14496-12:2015(E) 12.7 Font media (p186)
#define MOV_CLCP	MOV_TAG('c', 'l', 'c', 'p') // ClosedCaptionHandler
#define MOV_ALIS	MOV_TAG('a', 'l', 'i', 's') // Apple QuickTime Macintosh alias

// https://developer.apple.com/library/content/documentation/General/Reference/HLSAuthoringSpec/Requirements.html#//apple_ref/doc/uid/TP40016596-CH2-SW1
// Video encoding requirements 1.10: Use ¡®avc1¡¯, ¡®hvc1¡¯, or ¡®dvh1¡¯ rather than ¡®avc3¡¯, ¡®hev1¡¯, or ¡®dvhe¡¯
#define MOV_H264 MOV_TAG('a', 'v', 'c', '1') // H.264 ISO/IEC 14496-15:2010(E) 5.3.4 AVC Video Stream Definition (18)
#define MOV_HEVC MOV_TAG('h', 'v', 'c', '1') // H.265
#define MOV_MP4V MOV_TAG('m', 'p', '4', 'v') // MPEG-4 Video
#define MOV_MP4A MOV_TAG('m', 'p', '4', 'a') // AAC
#define MOV_MP4S MOV_TAG('m', 'p', '4', 's') // ISO/IEC 14496-14:2003(E) 5.6 Sample Description Boxes (p14)

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
	MOV_BRAND_DASH = MOV_TAG('d', 'a', 's', 'h'), // MPEG-DASH
	MOV_BRAND_MSDH = MOV_TAG('m', 's', 'd', 'h'), // MPEG-DASH
	MOV_BRAND_MSIX = MOV_TAG('m', 's', 'i', 'x'), // MPEG-DASH
};

#define MOV_TREX_FLAG_IS_LEADING_MASK					0x0C000000
#define MOV_TREX_FLAG_SAMPLE_DEPENDS_ON_MASK			0x03000000
#define MOV_TREX_FLAG_SAMPLE_IS_DEPENDED_ON_MASK		0x00C00000
#define MOV_TREX_FLAG_SAMPLE_HAS_REDUNDANCY_MASK		0x00300000
#define MOV_TREX_FLAG_SAMPLE_PADDING_VALUE_MASK			0x000E0000
#define MOV_TREX_FLAG_SAMPLE_IS_NO_SYNC_SAMPLE			0x00010000
#define MOV_TREX_FLAG_SAMPLE_DEGRADATION_PRIORITY_MASK	0x0000FFFF

// 8.6.4 Independent and Disposable Samples Box (p55)
#define MOV_TREX_FLAG_SAMPLE_DEPENDS_ON_I_PICTURE       0x02000000
#define MOV_TREX_FLAG_SAMPLE_DEPENDS_ON_NOT_I_PICTURE   0x01000000

#define MOV_TFHD_FLAG_BASE_DATA_OFFSET					0x00000001
#define MOV_TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX			0x00000002
#define MOV_TFHD_FLAG_DEFAULT_DURATION					0x00000008
#define MOV_TFHD_FLAG_DEFAULT_SIZE						0x00000010
#define MOV_TFHD_FLAG_DEFAULT_FLAGS						0x00000020
#define MOV_TFHD_FLAG_DURATION_IS_EMPTY					0x00010000
#define MOV_TFHD_FLAG_DEFAULT_BASE_IS_MOOF				0x00020000

#define MOV_TRUN_FLAG_DATA_OFFSET_PRESENT						0x0001
#define MOV_TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT				0x0004
#define MOV_TRUN_FLAG_SAMPLE_DURATION_PRESENT					0x0100
#define MOV_TRUN_FLAG_SAMPLE_SIZE_PRESENT						0x0200
#define MOV_TRUN_FLAG_SAMPLE_FLAGS_PRESENT						0x0400
#define MOV_TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT	0x0800

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
	int64_t pts; // track mdhd timescale
	int64_t dts;

	void* data;
	uint64_t offset; // is a 32 or 64 bit integer that gives the offset of the start of a chunk into its containing media file.
	size_t bytes;

	uint32_t sample_description_index;
	uint32_t samples_per_chunk; // write only
	uint32_t first_chunk; // write only
};

struct mov_fragment_t
{
	uint64_t time;
	uint64_t offset; // moof offset
};

struct mov_track_t
{
	uint32_t tag; // MOV_H264/MOV_MP4A
	uint32_t handler_type; // MOV_VIDEO/MOV_AUDIO
	const char* handler_descr; // VideoHandler/SoundHandler/SubtitleHandler

	struct mov_tkhd_t tkhd;
	struct mov_mdhd_t mdhd;
	struct mov_stbl_t stbl;

	// 8.8 Movie Fragments
	struct mov_trex_t trex;
	struct mov_tfhd_t tfhd;
	struct mov_fragment_t* frags;
	size_t frag_count, frag_capacity;

	struct mov_stsd_t stsd;

	struct mov_elst_t* elst;
	size_t elst_count;
	
	struct mov_sample_t* samples;
	size_t sample_count;
	size_t sample_offset; // sample_capacity

    int64_t tfdt_dts; // tfdt baseMediaDecodeTime
    int64_t start_dts; // write fmp4 only
    uint64_t offset; // write only
};

struct mov_t
{
	struct mov_ioutil_t io;
	
	struct mov_ftyp_t ftyp;
	struct mov_mvhd_t mvhd;

	int flags;
	int header;
	uint64_t moof_offset; // last moof offset(from file begin)
    uint64_t implicit_offset;

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
int mov_read_cslg(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_stss(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_avcc(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_hvcc(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_tx3g(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_trex(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_leva(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_tfhd(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_trun(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_tfra(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_sidx(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_mfhd(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_tfdt(struct mov_t* mov, const struct mov_box_t* box);
int mov_read_mehd(struct mov_t* mov, const struct mov_box_t* box);

size_t mov_write_ftyp(const struct mov_t* mov);
size_t mov_write_mvhd(const struct mov_t* mov);
size_t mov_write_mdhd(const struct mov_t* mov);
size_t mov_write_tkhd(const struct mov_t* mov);
size_t mov_write_hdlr(const struct mov_t* mov);
size_t mov_write_vmhd(const struct mov_t* mov);
size_t mov_write_smhd(const struct mov_t* mov);
size_t mov_write_nmhd(const struct mov_t* mov);
size_t mov_write_sthd(const struct mov_t* mov);
size_t mov_write_dinf(const struct mov_t* mov);
size_t mov_write_dref(const struct mov_t* mov);
size_t mov_write_elst(const struct mov_t* mov);
size_t mov_write_stsd(const struct mov_t* mov);
size_t mov_write_stts(const struct mov_t* mov, uint32_t count);
size_t mov_write_ctts(const struct mov_t* mov, uint32_t count);
size_t mov_write_stco(const struct mov_t* mov, uint32_t count);
size_t mov_write_stss(const struct mov_t* mov);
size_t mov_write_stsc(const struct mov_t* mov);
size_t mov_write_stsz(const struct mov_t* mov);
size_t mov_write_esds(const struct mov_t* mov);
size_t mov_write_avcc(const struct mov_t* mov);
size_t mov_write_hvcc(const struct mov_t* mov);
size_t mov_write_tx3g(const struct mov_t* mov);
size_t mov_write_trex(const struct mov_t* mov);
size_t mov_write_tfhd(const struct mov_t* mov);
size_t mov_write_trun(const struct mov_t* mov, size_t from, size_t count, uint32_t offset);
size_t mov_write_tfra(const struct mov_t* mov);
size_t mov_write_styp(const struct mov_t* mov);
size_t mov_write_tfdt(const struct mov_t* mov);
size_t mov_write_sidx(const struct mov_t* mov, uint64_t offset);
size_t mov_write_mfhd(const struct mov_t* mov, uint32_t fragment);
size_t mov_write_edts(const struct mov_t* mov);
size_t mov_write_stbl(const struct mov_t* mov);
size_t mov_write_minf(const struct mov_t* mov);
size_t mov_write_mdia(const struct mov_t* mov);
size_t mov_write_trak(const struct mov_t* mov);

uint32_t mov_build_stts(struct mov_track_t* track);
uint32_t mov_build_ctts(struct mov_track_t* track);
uint32_t mov_build_stco(struct mov_track_t* track);
void mov_apply_stco(struct mov_track_t* track);
void mov_apply_elst(struct mov_track_t *track);
void mov_apply_stts(struct mov_track_t* track);
void mov_apply_ctts(struct mov_track_t* track);
void mov_apply_stss(struct mov_track_t* track);
void mov_apply_elst_tfdt(struct mov_track_t *track);

void mov_write_size(const struct mov_t* mov, uint64_t offset, size_t size);

size_t mov_stco_size(const struct mov_track_t* track, uint64_t offset);

uint8_t mov_tag_to_object(uint32_t tag);
uint32_t mov_object_to_tag(uint8_t object);

void mov_free_track(struct mov_track_t* track);
struct mov_track_t* mov_add_track(struct mov_t* mov);
struct mov_track_t* mov_find_track(const struct mov_t* mov, uint32_t track);
struct mov_track_t* mov_fetch_track(struct mov_t* mov, uint32_t track); // find and add
int mov_add_audio(struct mov_track_t* track, const struct mov_mvhd_t* mvhd, uint32_t timescale, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size);
int mov_add_video(struct mov_track_t* track, const struct mov_mvhd_t* mvhd, uint32_t timescale, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size);
int mov_add_subtitle(struct mov_track_t* track, const struct mov_mvhd_t* mvhd, uint32_t timescale, uint8_t object, const void* extra_data, size_t extra_data_size);

#endif /* !_mov_internal_h_ */
