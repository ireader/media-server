#ifndef _mpeg_ts_proto_h_
#define _mpeg_ts_proto_h_

#include "mpeg-types.h"
#include "mpeg-pes-proto.h"

#define TS_PACKET_SIZE		188

#define TS_SYNC_BYTE        0x47

struct ts_adaptation_field_t
{
	unsigned int adaptation_field_length : 8;
	unsigned int discontinuity_indicator : 1;
	unsigned int random_access_indicator : 1;
	unsigned int elementary_stream_priority_indicator : 1;
	unsigned int PCR_flag : 1;
	unsigned int OPCR_flag : 1;
	unsigned int splicing_point_flag : 1;
	unsigned int transport_private_data_flag : 1;
	unsigned int adaptation_field_extension_flag : 1;

	int64_t program_clock_reference_base; // 33-bits
	unsigned int program_clock_reference_extension; // 9-bits

	int64_t original_program_clock_reference_base; // 33-bits
	unsigned int original_program_clock_reference_extension; // 9-bits

	unsigned int splice_countdown : 8;

	unsigned int transport_private_data_length : 8;

	unsigned int adaptation_field_extension_length : 8;
	unsigned int ltw_flag : 1;
	unsigned int piecewise_rate_flag : 1;
	unsigned int seamless_splice_flag : 1;

	unsigned int ltw_valid_flag : 1;
	unsigned int ltw_offset : 15;

	unsigned int piecewise_rate : 22;

	unsigned int Splice_type : 4;
	int64_t DTS_next_AU;
};

struct ts_packet_header_t
{
	unsigned int transport_error_indicator : 1;
	unsigned int payload_unit_start_indicator : 1;
	unsigned int transport_priority : 1;

	unsigned int transport_scrambling_control : 2;
	unsigned int adaptation_field_control : 2;
	unsigned int continuity_counter : 4;

	struct ts_adaptation_field_t adaptation;
};

struct pmt_t
{
	unsigned int pid;		// PID : 13 [0x0010, 0x1FFE]
	unsigned int pn;		// program_number: 16 [1, 0xFFFF]
	unsigned int ver;		// version_number : 5
	unsigned int cc;		// continuity_counter : 4
	unsigned int PCR_PID;	// 13-bits
	unsigned int pminfo_len;// program_info_length : 12
	uint8_t* pminfo;	// program_info;
    
    char provider[64];
    char name[64];

	unsigned int stream_count;
	struct pes_t streams[4];
};

struct pat_t
{
	unsigned int tsid;	// transport_stream_id : 16;
	unsigned int ver;	// version_number : 5;
	unsigned int cc;	//continuity_counter : 4;

	unsigned int pmt_count;
	struct pmt_t pmts[4];
};

// Table 2-3 ¨C PID table(p36)
enum ETS_PID
{
	TS_PID_PAT	= 0x00, // program association table
	TS_PID_CAT	= 0x01, // conditional access table
	TS_PID_TSDT	= 0x02, // transport stream description table
	TS_PID_IPMP	= 0x03, // IPMP control information table
	// 0x0004-0x000F Reserved
	// 0x0010-0x1FFE May be assigned as network_PID, Program_map_PID, elementary_PID, or for other purposes
    TS_PID_SDT  = 0x11, // https://en.wikipedia.org/wiki/Service_Description_Table / https://en.wikipedia.org/wiki/MPEG_transport_stream
	TS_PID_USER	= 0x0042,
	TS_PID_NULL	= 0x1FFF, // Null packet
};

// 2.4.4.4 Table_id assignments
// Table 2-31 ¨C table_id assignment values(p66/p39)
enum EPAT_TID
{
	PAT_TID_PAS				= 0x00, // program_association_section
	PAT_TID_CAS				= 0x01, // conditional_access_section(CA_section)
	PAT_TID_PMS				= 0x02, // TS_program_map_section
	PAT_TID_SDS				= 0x03, // TS_description_section
	PAT_TID_MPEG4_scene		= 0x04, // ISO_IEC_14496_scene_description_section
	PAT_TID_MPEG4_object	= 0x05, // ISO_IEC_14496_object_descriptor_section
	PAT_TID_META			= 0x06, // Metadata_section
	PAT_TID_IPMP			= 0x07, // IPMP_Control_Information_section(defined in ISO/IEC 13818-11)
	PAT_TID_H222			= 0x08, // Rec. ITU-T H.222.0 | ISO/IEC 13818-1 reserved
	PAT_TID_USER			= 0x40,	// User private
    PAT_TID_SDT             = 0x42, // service_description_section 
	PAT_TID_Forbidden		= 0xFF,
};

// ISO/IEC 13818-1:2015 (E)
// 2.4.4.9 Semantic definition of fields in transport stream program map section
// Table 2-34 ¨C Stream type assignments(p69)
enum EPSI_STREAM_TYPE
{
	PSI_STREAM_RESERVED			= 0x00, // ITU-T | ISO/IEC Reserved
	PSI_STREAM_MPEG1			= 0x01, // ISO/IEC 11172-2 Video
	PSI_STREAM_MPEG2			= 0x02, // Rec. ITU-T H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream(see Note 2)
	PSI_STREAM_AUDIO_MPEG1		= 0x03, // ISO/IEC 11172-3 Audio
	PSI_STREAM_MP3				= 0x04, // ISO/IEC 13818-3 Audio
	PSI_STREAM_PRIVATE_SECTION	= 0x05, // Rec. ITU-T H.222.0 | ISO/IEC 13818-1 private_sections
	PSI_STREAM_PRIVATE_DATA		= 0x06, // Rec. ITU-T H.222.0 | ISO/IEC 13818-1 PES packets containing private data
	PSI_STREAM_MHEG				= 0x07, // ISO/IEC 13522 MHEG
	PSI_STREAM_DSMCC			= 0x08, // Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Annex A DSM-CC
	PSI_STREAM_H222_ATM			= 0x09, // Rec. ITU-T H.222.1
	PSI_STREAM_DSMCC_A			= 0x0a, // ISO/IEC 13818-6(Extensions for DSM-CC) type A
	PSI_STREAM_DSMCC_B			= 0x0b, // ISO/IEC 13818-6(Extensions for DSM-CC) type B
	PSI_STREAM_DSMCC_C			= 0x0c, // ISO/IEC 13818-6(Extensions for DSM-CC) type C
	PSI_STREAM_DSMCC_D			= 0x0d, // ISO/IEC 13818-6(Extensions for DSM-CC) type D
	PSI_STREAM_H222_Aux			= 0x0e, // Rec. ITU-T H.222.0 | ISO/IEC 13818-1 auxiliary
	PSI_STREAM_AAC				= 0x0f, // ISO/IEC 13818-7 Audio with ADTS transport syntax
	PSI_STREAM_MPEG4			= 0x10, // ISO/IEC 14496-2 Visual
	PSI_STREAM_MPEG4_AAC_LATM	= 0x11, // ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3
	PSI_STREAM_MPEG4_PES		= 0x12, // ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets
	PSI_STREAM_MPEG4_SECTIONS	= 0x13, // ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC 14496_sections
	PSI_STREAM_MPEG2_SDP		= 0x14, // ISO/IEC 13818-6 Synchronized Download Protocol
	PSI_STREAM_PES_META			= 0x15, // Metadata carried in PES packets
	PSI_STREAM_SECTION_META		= 0x16, // Metadata carried in metadata_sections
	PSI_STREAM_DSMCC_DATA		= 0x17, // Metadata carried in ISO/IEC 13818-6 Data Carousel
	PSI_STREAM_DSMCC_OBJECT		= 0x18, // Metadata carried in ISO/IEC 13818-6 Object Carousel
	PSI_STREAM_DSMCC_SDP		= 0x19, // Metadata carried in ISO/IEC 13818-6 Synchronized Download Protocol
	PSI_STREAM_MPEG2_IPMP		= 0x1a, // IPMP stream (defined in ISO/IEC 13818-11, MPEG-2 IPMP)
	PSI_STREAM_H264				= 0x1b, // H.264
	PSI_STREAM_MPEG4_AAC		= 0x1c, // ISO/IEC 14496-3 Audio, without using any additional transport syntax, such as DST, ALS and SLS
	PSI_STREAM_MPEG4_TEXT		= 0x1d, // ISO/IEC 14496-17 Text
	PSI_STREAM_AUX_VIDEO		= 0x1e, // Auxiliary video stream as defined in ISO/IEC 23002-3
	PSI_STREAM_H264_SVC			= 0x1f, // SVC video sub-bitstream of an AVC video stream conforming to one or more profiles defined in Annex G of Rec. ITU-T H.264 | ISO/IEC 14496-10
	PSI_STREAM_H264_MVC			= 0x20, // MVC video sub-bitstream of an AVC video stream conforming to one or more profiles defined in Annex H of Rec. ITU-T H.264 | ISO/IEC 14496-10
	PSI_STREAM_JPEG_2000		= 0x21, // Video stream conforming to one or more profiles as defined in Rec. ITU-T T.800 | ISO/IEC 15444-1
	PSI_STREAM_MPEG2_3D			= 0x22, // Additional view Rec. ITU-T H.262 | ISO/IEC 13818-2 video stream for service-compatible stereoscopic 3D services
	PSI_STREAM_MPEG4_3D			= 0x23, // Additional view Rec. ITU-T H.264 | ISO/IEC 14496-10 video stream conforming to one or more profiles defined in Annex A for service-compatible stereoscopic 3D services
	PSI_STREAM_H265				= 0x24, // Rec. ITU-T H.265 | ISO/IEC 23008-2 video stream or an HEVC temporal video sub-bitstream
	PSI_STREAM_H265_subset		= 0x25, // HEVC temporal video subset of an HEVC video stream conforming to one or more profiles defined in Annex A of Rec. ITU-T H.265 | ISO/IEC 23008-2
	PSI_STREAM_H264_MVCD		= 0x26, // MVCD video sub-bitstream of an AVC video stream conforming to one or more profiles defined in Annex I of Rec. ITU-T H.264 | ISO/IEC 14496-10
	// 0x27-0x7E Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Reserved
	PSI_STREAM_IPMP				= 0x7F, // IPMP stream
	// 0x80-0xFF User Private
	PSI_STREAM_VIDEO_CAVS		= 0x42, // ffmpeg/libavformat/mpegts.h
	PSI_STREAM_AUDIO_AC3		= 0x81, // ffmpeg/libavformat/mpegts.h
    PSI_STREAM_AUDIO_EAC3       = 0x87, // ffmpeg/libavformat/mpegts.h
	PSI_STREAM_AUDIO_DTS		= 0x8a, // ffmpeg/libavformat/mpegts.h
	PSI_STREAM_VIDEO_DIRAC		= 0xd1, // ffmpeg/libavformat/mpegts.h
	PSI_STREAM_VIDEO_VC1		= 0xea, // ffmpeg/libavformat/mpegts.h
	PSI_STREAM_VIDEO_SVAC		= 0x80, // GBT 25724-2010 SVAC(2014)
	PSI_STREAM_AUDIO_SVAC		= 0x9B, // GBT 25724-2010 SVAC(2014)
	PSI_STREAM_AUDIO_G711A		= 0x90,
    PSI_STREAM_AUDIO_G711U      = 0x91,
	PSI_STREAM_AUDIO_G722		= 0x92,
	PSI_STREAM_AUDIO_G723		= 0x93,
	PSI_STREAM_AUDIO_G729		= 0x99,
};

enum
{
    MPEG_FLAG_IDR_FRAME          = 0x0001,
    MPEG_FLAG_H264_H265_WITH_AUD = 0x8000,
};

struct pmt_t* pat_find(struct pat_t* pat, uint16_t pn);
size_t pat_read(struct pat_t *pat, const uint8_t* data, size_t bytes);
size_t pat_write(const struct pat_t *pat, uint8_t *data);
size_t pmt_read(struct pmt_t *pmt, const uint8_t* data, size_t bytes);
size_t pmt_write(const struct pmt_t *pmt, uint8_t *data);
size_t sdt_read(struct pat_t *pat, const uint8_t* data, size_t bytes);

#endif /* !_mpeg_ts_proto_h_ */
