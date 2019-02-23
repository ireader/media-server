#ifndef _mpeg_pes_dec_h_
#define _mpeg_pes_dec_h_

#include "mpeg-types.h"

// ISO/IEC 13818-1:2015 (E)
// 2.4.3.7 Semantic definition of fields in PES packet
// Table 2-22 ¨C Stream_id assignments(p54)
// In transport streams, the stream_id may be set to any valid value which correctly describes the elementary stream type as defined in Table 2-22. 
// In transport streams, the elementary stream type is specified in the program-specific information as specified in 2.4.4
enum EPES_STREAM_ID
{
	PES_SID_SUB			= 0x20, // ffmpeg/libavformat/mpeg.h
	PES_SID_AC3			= 0x80, // ffmpeg/libavformat/mpeg.h
	PES_SID_DTS			= 0x88, // ffmpeg/libavformat/mpeg.h
	PES_SID_LPCM		= 0xA0, // ffmpeg/libavformat/mpeg.h

	PES_SID_EXTENSION	= 0xB7, // PS system_header extension(p81)
	PES_SID_END			= 0xB9, // MPEG_program_end_code
	PES_SID_START		= 0xBA, // Pack start code
	PES_SID_SYS			= 0xBB, // System header start code

	PES_SID_PSM			= 0xBC, // program_stream_map
	PES_SID_PRIVATE_1	= 0xBD, // private_stream_1
	PES_SID_PADDING		= 0xBE, // padding_stream
	PES_SID_PRIVATE_2	= 0xBF, // private_stream_2
	PES_SID_AUDIO		= 0xC0, // ISO/IEC 13818-3/11172-3/13818-7/14496-3 audio stream '110x xxxx'
	PES_SID_VIDEO		= 0xE0, // H.262 | H.264 | H.265 | ISO/IEC 13818-2/11172-2/14496-2/14496-10 video stream '1110 xxxx'
	PES_SID_ECM			= 0xF0, // ECM_stream
	PES_SID_EMM			= 0xF1, // EMM_stream
	PES_SID_DSMCC		= 0xF2, // H.222.0 | ISO/IEC 13818-1/13818-6_DSMCC_stream
	PES_SID_13522		= 0xF3, // ISO/IEC_13522_stream
	PES_SID_H222_A		= 0xF4, // Rec. ITU-T H.222.1 type A
	PES_SID_H222_B		= 0xF5, // Rec. ITU-T H.222.1 type B
	PES_SID_H222_C		= 0xF6, // Rec. ITU-T H.222.1 type C
	PES_SID_H222_D		= 0xF7, // Rec. ITU-T H.222.1 type D
	PES_SID_H222_E		= 0xF8, // Rec. ITU-T H.222.1 type E
	PES_SID_ANCILLARY	= 0xF9, // ancillary_stream
	PES_SID_MPEG4_SL	= 0xFA, // ISO/IEC 14496-1_SL_packetized_stream
	PES_SID_MPEG4_Flex	= 0xFB, // ISO/IEC 14496-1_FlexMux_stream
	PES_SID_META		= 0xFC, // metadata stream
	PES_SID_EXTEND		= 0xFD,	// extended_stream_id
	PES_SID_RESERVED	= 0xFE,	// reserved data stream
	PES_SID_PSD			= 0xFF, // program_stream_directory
};

struct packet_t
{
    uint8_t sid;
    uint8_t codecid;

    int flags;
    int64_t pts;
    int64_t dts;
    uint8_t *data;
    size_t size;
    size_t capacity;
};

struct pes_t
{
    uint16_t pn;        // TS program number(0-ps)
	uint16_t pid;		// PES PID : 13
	uint8_t sid;		// PES stream_id : 8
	uint8_t codecid;	// PMT/PSM stream_type : 8
	uint8_t cc;			// continuity_counter : 4;
	uint8_t* esinfo;	// es_info
	uint16_t esinfo_len;// es_info_length : 12

	uint32_t len;		// PES_packet_length : 16;

	uint32_t reserved10 : 2;
	uint32_t PES_scrambling_control : 2;
	uint32_t PES_priority : 1;
	uint32_t data_alignment_indicator : 1;
	uint32_t copyright : 1;
	uint32_t original_or_copy : 1;

	uint32_t PTS_DTS_flags : 2;
	uint32_t ESCR_flag : 1;
	uint32_t ES_rate_flag : 1;
	uint32_t DSM_trick_mode_flag : 1;
	uint32_t additional_copy_info_flag : 1;
	uint32_t PES_CRC_flag : 1;
	uint32_t PES_extension_flag : 1;
	uint32_t PES_header_data_length : 8;

	int64_t pts;
	int64_t dts;
	int64_t ESCR_base;
	uint32_t ESCR_extension;
	uint32_t ES_rate;

	//uint8_t trick_mode;
	//uint32_t trick_mode_control : 3;
	//uint32_t field_id : 2;
	//uint32_t intra_slice_refresh : 1;
	//uint32_t frequency_truncation : 2;

	//uint8_t additional_copy_info;
	//int16_t previous_PES_packet_CRC;

	//uint32_t PES_private_data_flag : 1;
	//uint32_t pack_header_field_flag : 1;
	//uint32_t program_packet_sequence_counter_flag : 1;
	//uint32_t P_STD_buffer_flag : 1;
	//uint32_t reserved_ : 3;
	//uint32_t PES_extension_flag_2 : 1;
	//uint32_t PES_private_data_flag2 : 1;
	//uint8_t PES_private_data[128/8];

	//uint32_t pack_field_length : 8;

    struct packet_t pkt;
};

size_t pes_read_header(struct pes_t *pes, const uint8_t* data, size_t bytes);
size_t pes_write_header(const struct pes_t *pes, uint8_t* data, size_t bytes);
size_t pes_read_mpeg1_header(struct pes_t *pes, const uint8_t* data, size_t bytes);

typedef int (*pes_packet_handler)(void* param, int program, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes);
int pes_packet(struct packet_t* pkt, const struct pes_t* pes, const void* data, size_t size, pes_packet_handler handler, void* param);

#endif /* !_mpeg_pes_dec_h_ */
