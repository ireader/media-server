#ifndef _mpeg_ps_proto_h_
#define _mpeg_ps_proto_h_

#include "mpeg-types.h"

#define NSTREAM 48	// 32-audio('110xxxxx') + 16-video('1110xxxx')
#define N_ACCESS_UNIT 16

#define SEQUENCE_END_CODE		(0x000001B7)
#define MPEG_PROGRAM_END_CODE	(0x000001B9)

typedef struct _ps_packet_header_t
{
	int64_t system_clock_reference_base;
	uint32_t system_clock_reference_extension;

	uint32_t program_mux_rate;
} ps_packet_header_t;

typedef struct _ps_stream_header_t
{
	uint32_t stream_id : 8;
	uint32_t stream_extid : 8;
	uint32_t buffer_bound_scale : 1;
	uint32_t buffer_size_bound : 13;
} ps_stream_header_t;

typedef struct _ps_system_header_t
{
	uint32_t rate_bound;
	uint32_t audio_bound : 6;
	uint32_t fixed_flag : 1;
	uint32_t CSPS_flag : 1;
	uint32_t system_audio_lock_flag : 1;
	uint32_t system_video_lock_flag : 1;
	uint32_t video_bound : 5;
	uint32_t packet_rate_restriction_flag : 1;

	uint32_t stream_count;
	ps_stream_header_t streams[NSTREAM];
} ps_system_header_t;

typedef struct _psm_t
{
	uint32_t ver : 5;	// version_number : 5;
	
	struct stream_t
	{
		uint8_t avtype; // stream_type
		uint8_t pesid; // pes stream_id
		uint8_t *esinfo;
		uint16_t esinfo_len;
	} streams[NSTREAM];
	size_t stream_count;
} psm_t;

typedef struct _psd_t
{
	uint64_t prev_directory_offset;
	uint64_t next_directory_offset;

	struct access_unit_t
	{
		uint8_t packet_stream_id;
		uint8_t pes_header_position_offset_sign;
		uint64_t PTS;
		uint64_t pes_header_position_offset;
		uint16_t reference_offset;
		uint32_t bytes_to_read;
		uint8_t packet_stream_id_extension_msbs;
		uint8_t packet_stream_id_extension_lsbs;
		uint8_t intra_coded_indicator;
		uint8_t coding_parameters_indicator;
	} units[N_ACCESS_UNIT];
} psd_t;

size_t psm_read(const uint8_t* data, size_t bytes, psm_t *psm);
size_t psm_write(const psm_t *psm, uint8_t *data);

size_t psd_read(const uint8_t* data, size_t bytes, psd_t *psd);

#endif /* !_mpeg_ps_proto_h_ */
