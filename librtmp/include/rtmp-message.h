#ifndef _rtmp_message_h_
#define _rtmp_message_h_

#include <stdint.h>
#include <stddef.h>

enum
{
	RTMP_CHUNK_TYPE_0 = 0, // 11-bytes: timestamp + length + stream type + stream id
	RTMP_CHUNK_TYPE_1 = 1, // 7-bytes: delta + length + stream type
	RTMP_CHUNK_TYPE_2 = 2, // 3-bytes: delta
	RTMP_CHUNK_TYPE_3 = 3, // 0-byte
};

struct rtmp_chunk_header_t
{
	uint8_t fmt; // RTMP_CHUNK_TYPE_XXX
	uint32_t cid; // chunk stream id(22-bits)

	uint32_t timestamp; // delta(24-bits) / extended timestamp(32-bits)

	uint32_t length; // message length (24-bits)
	uint8_t type; // message type id

	uint32_t stream_id; // message stream id
};

#endif /* !_rtmp_message_h_ */
