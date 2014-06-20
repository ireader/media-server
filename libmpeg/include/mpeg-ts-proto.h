#ifndef _mpeg_ts_proto_h_
#define _mpeg_ts_proto_h_

typedef __int64 int64_t;

typedef struct _ts_adaptation_field_t
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

	int64_t program_clock_reference_base;
	unsigned int program_clock_reference_extension;

	int64_t original_program_clock_reference_base;
	unsigned int original_program_clock_reference_extension;

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
} ts_adaptation_field_t;

typedef struct _ts_packet_header_t
{
	unsigned int transport_scrambling_control : 2;
	unsigned int adaptation_field_control : 2;
	unsigned int continuity_counter : 4;

	ts_adaptation_field_t adaptation;
} ts_packet_header_t;

typedef struct _ts_pes_t
{
	struct _ts_pmt_t *pmt; // program map table

	int pid;				// PID : 13
	int sid;				// stream_type : 8
	int cc;					// continuity_counter : 4;
	int info_length;		// es_info_length : 12
	unsigned char* info;	// es_info

	int len;				// PES_packet_length : 16;

	unsigned int reserved10 : 2;
	unsigned int PES_scrambling_control : 2;
	unsigned int PES_priority : 1;
	unsigned int data_alignment_indicator : 1;
	unsigned int copyright : 1;
	unsigned int original_or_copy : 1;

	unsigned int PTS_DTS_flags : 2;
	unsigned int ESCR_flag : 1;
	unsigned int ES_rate_flag : 1;
	unsigned int DSM_trick_mode_flag : 1;
	unsigned int additional_copy_info_flag : 1;
	unsigned int PES_CRC_flag : 1;
	unsigned int PES_extension_flag : 1;
	unsigned int PES_header_data_length : 8;

	int64_t pts;
	int64_t dts;
	int64_t ESCR_base;
	unsigned int ESCR_extension;
	unsigned int ES_rate;

	//unsigned char trick_mode;
	//unsigned int trick_mode_control : 3;
	//unsigned int field_id : 2;
	//unsigned int intra_slice_refresh : 1;
	//unsigned int frequency_truncation : 2;

	//unsigned char additional_copy_info;
	//unsigned short previous_PES_packet_CRC;

	//unsigned int PES_private_data_flag : 1;
	//unsigned int pack_header_field_flag : 1;
	//unsigned int program_packet_sequence_counter_flag : 1;
	//unsigned int P_STD_buffer_flag : 1;
	//unsigned int reserved_ : 3;
	//unsigned int PES_extension_flag_2 : 1;
	//unsigned int PES_private_data_flag2 : 1;
	//unsigned char PES_private_data[128/8];

	//unsigned int pack_field_length : 8;
} ts_pes_t;

typedef struct _ts_pmt_t
{
	int pid;	// PID : 13
	int pn;		// program_number: 16
	int ver;	// version_number : 5
	int cc;		// continuity_counter : 4
	int PCR_PID; // 13-bits
	int program_info_length; // 12-bits
	unsigned char* program_info;

	int stream_count;
	ts_pes_t *streams;
} ts_pmt_t;

typedef struct _ts_pat_t
{
	int tsid; // transport_stream_id : 16;
	int ver; // version_number : 5;
	int cc;	//continuity_counter : 4;

	int pmt_count;
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
