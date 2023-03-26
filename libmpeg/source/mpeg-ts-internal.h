#ifndef _mpeg_ts_internal_h_
#define _mpeg_ts_internal_h_

#include "mpeg-proto.h"
#include "mpeg-types.h"
#include "mpeg-pes-internal.h"
#include "mpeg-util.h"

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
	unsigned int pmt_capacity;
	struct pmt_t pmt_default[1];
	struct pmt_t* pmts;
};

struct pmt_t* pat_alloc_pmt(struct pat_t* pat);
struct pmt_t* pat_find(struct pat_t* pat, uint16_t pn);
size_t pat_read(struct pat_t *pat, const uint8_t* data, size_t bytes);
size_t pat_write(const struct pat_t *pat, uint8_t *data);
size_t pmt_read(struct pmt_t *pmt, const uint8_t* data, size_t bytes);
size_t pmt_write(const struct pmt_t *pmt, uint8_t *data);
size_t sdt_read(struct pat_t *pat, const uint8_t* data, size_t bytes);
size_t sdt_write(const struct pat_t* pat, uint8_t* data);
void pat_clear(struct pat_t* pat);
void pmt_clear(struct pmt_t* pmt);

#endif /* !_mpeg_ts_internal_h_ */
