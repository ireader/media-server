#ifndef _mpeg_pes_internal_h_
#define _mpeg_pes_internal_h_

#include "mpeg-proto.h"
#include "mpeg-types.h"
#include "mpeg-util.h"

enum {
	MPEG_ERROR_NEED_MORE_DATA = 0,
	MPEG_ERROR_OK,
	MPEG_ERROR_INVALID_DATA,
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

	int vcl; // h.264/h.265 only
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

	int have_pes_header; // TS demuxer only
	int flags; // TS/PS demuxer only
    struct packet_t pkt;
};

int pes_read_header(struct pes_t *pes, struct mpeg_bits_t* reader);
int pes_read_mpeg1_header(struct pes_t* pes, struct mpeg_bits_t* reader);
size_t pes_write_header(const struct pes_t *pes, uint8_t* data, size_t bytes);

typedef int (*pes_packet_handler)(void* param, int program, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes);
int pes_packet(struct packet_t* pkt, struct pes_t* pes, const void* data, size_t size, size_t* consume, int start, pes_packet_handler handler, void* param);

uint16_t mpeg_bits_read15(struct mpeg_bits_t* reader);
uint32_t mpeg_bits_read30(struct mpeg_bits_t* reader);
uint64_t mpeg_bits_read45(struct mpeg_bits_t* reader);

#endif /* !_mpeg_pes_internal_h_ */
