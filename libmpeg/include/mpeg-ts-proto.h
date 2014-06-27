#ifndef _mpeg_ts_proto_h_
#define _mpeg_ts_proto_h_

#define PTS_NO_VALUE (int64_t)0x8000000000000000L

typedef unsigned char	uint8_t;
typedef short			int16_t;
typedef unsigned short	uint16_t;
typedef int				int32_t;
typedef unsigned int	uint32_t;

#if defined(OS_WINDOWS)
	typedef __int64				int64_t;
	typedef unsigned __int64	uint64_t;
#else
	typedef long long			int64_t;
	typedef unsigned long long	uint64_t;
#endif

typedef struct _ts_adaptation_field_t
{
	uint32_t adaptation_field_length : 8;
	uint32_t discontinuity_indicator : 1;
	uint32_t random_access_indicator : 1;
	uint32_t elementary_stream_priority_indicator : 1;
	uint32_t PCR_flag : 1;
	uint32_t OPCR_flag : 1;
	uint32_t splicing_point_flag : 1;
	uint32_t transport_private_data_flag : 1;
	uint32_t adaptation_field_extension_flag : 1;

	int64_t program_clock_reference_base;
	uint32_t program_clock_reference_extension;

	int64_t original_program_clock_reference_base;
	uint32_t original_program_clock_reference_extension;

	uint32_t splice_countdown : 8;

	uint32_t transport_private_data_length : 8;

	uint32_t adaptation_field_extension_length : 8;
	uint32_t ltw_flag : 1;
	uint32_t piecewise_rate_flag : 1;
	uint32_t seamless_splice_flag : 1;

	uint32_t ltw_valid_flag : 1;
	uint32_t ltw_offset : 15;

	uint32_t piecewise_rate : 22;

	uint32_t Splice_type : 4;
	int64_t DTS_next_AU;
} ts_adaptation_field_t;

typedef struct _ts_packet_header_t
{
	uint32_t transport_scrambling_control : 2;
	uint32_t adaptation_field_control : 2;
	uint32_t continuity_counter : 4;

	ts_adaptation_field_t adaptation;
} ts_packet_header_t;

typedef struct _ts_pes_t
{
	struct _ts_pmt_t *pmt;	// program map table

	uint32_t pid;		// PID : 13
	uint32_t sid;		// stream_type : 8
	uint32_t cc;		// continuity_counter : 4;
	uint32_t esinfo_len;// es_info_length : 12
	uint8_t* esinfo;	// es_info

	uint32_t len;	// PES_packet_length : 16;

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
	uint8_t *payload;
	size_t payload_len;
} ts_pes_t;

typedef struct _ts_pmt_t
{
	uint32_t pid;		// PID : 13
	uint32_t pn;		// program_number: 16
	uint32_t ver;		// version_number : 5
	uint32_t cc;		// continuity_counter : 4
	uint32_t PCR_PID;	// 13-bits
	uint32_t pminfo_len;// program_info_length : 12
	uint8_t* pminfo;	// program_info;

	uint32_t stream_count;
	ts_pes_t *streams;
} ts_pmt_t;

typedef struct _ts_pat_t
{
	uint32_t tsid; // transport_stream_id : 16;
	uint32_t ver; // version_number : 5;
	uint32_t cc;	//continuity_counter : 4;

	uint32_t pmt_count;
	ts_pmt_t *pmt;
} ts_pat_t;

enum EPES_STREAM_ID
{
	PES_PROGRAM_STREAM_MAP = 0xBC,
	PES_PRIVATE_STREAM_1 = 0xBD,
	PES_PADDING_STREAM = 0xBE,
	PES_PRIVATE_STREAM_2 = 0xBF,
	PES_AUDIO_STREAM = 0xC0,
	PES_VIDEO_STREAM = 0xE0,
	PES_ECM = 0xF0,
	PES_EMM = 0xF1,
	PES_PROGRAM_STREAM_DIRECTORY = 0xFF,
	PES_DSMCC_STREAM = 0xF2,
	PES_H222_E_STREAM = 0xF8,
};

enum ESTREAM_ID
{
	STREAM_VIDEO_MPEG1		= 0x01,
	STREAM_VIDEO_MPEG2		= 0x02,
	STREAM_VIDEO_MPEG4		= 0x10,
	STREAM_VIDEO_H264		= 0x1b,
	STREAM_VIDEO_VC1		= 0xea,
	STREAM_VIDEO_DIRAC		= 0xd1,

	STREAM_AUDIO_MPEG1		= 0x03,
	STREAM_AUDIO_MPEG2		= 0x04,
	STREAM_AUDIO_AAC		= 0x0f,
	STREAM_AUDIO_AAC_LATM	= 0x11,
	STREAM_AUDIO_AC3		= 0x81,
	STREAM_AUDIO_DTS		= 0x8a,

	STREAM_PRIVATE_SECTION	= 0x05,
	STREAM_PRIVATE_DATA		= 0x06,
};

#endif /* !_mpeg_ts_proto_h_ */
