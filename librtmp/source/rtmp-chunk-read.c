#include "rtmp-internal.h"
#include "rtmp-util.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>

// 5.3.1. Chunk Format (p11)
/* 3-bytes basic header + 11-bytes message header + 4-bytes extended timestamp */
#define MAX_CHUNK_HEADER 18

#define MIN(x, y) ((x) < (y) ? (x) : (y))

enum rtmp_state_t
{
	RTMP_STATE_INIT = 0,
	RTMP_STATE_BASIC_HEADER,
	RTMP_STATE_MESSAGE_HEADER,
	RTMP_STATE_EXTENDED_TIMESTAMP,
	RTMP_STATE_PAYLOAD,
};

struct rtmp_parse_t
{
	uint8_t header[MAX_CHUNK_HEADER];
	uint32_t basic_bytes; // basic header length
	uint32_t bytes;

	uint8_t* payload;
	uint32_t payloadSize;
	uint32_t capacity;

	enum rtmp_state_t state;
};

static int rtmp_read_basic_header(const uint8_t* data, uint8_t* fmt, uint32_t* cid)
{
	*fmt = data[0] >> 6;
	*cid = data[0] & 0x3F;

	if (0 == *cid)
	{
		*cid = 64 + (uint32_t)data[1];
		return 2;
	}
	else if (1 == *cid)
	{
		*cid = 64 + (uint32_t)data[1] + ((uint32_t)data[2] << 8) /* 256 */;
		return 3;
	}
	else
	{
		return 1;
	}
}

static int rtmp_read_message_header(const uint8_t* data, struct rtmp_chunk_header_t* header)
{
	int offset = 0;

	// timestamp / delta
	if (header->fmt <= RTMP_CHUNK_TYPE_2)
	{
		be_read_uint24(data + offset, &header->timestamp);
		offset += 3;
	}

	// message length + type
	if (header->fmt <= RTMP_CHUNK_TYPE_1)
	{
		be_read_uint24(data + offset, &header->length);
		header->type = data[offset + 3];
		offset += 4;
	}

	// message stream id
	if (header->fmt == RTMP_CHUNK_TYPE_0)
	{
		le_read_uint32(data + offset, &header->stream_id);
		offset += 4;
	}

	return offset;
}

static int rtmp_read_extended_timestamp(const uint8_t* out, uint32_t* timestamp)
{
	if (*timestamp >= 0xFFFFFF)
	{
		// extended timestamp
		be_read_uint32(out, timestamp);
		return 4;
	}
	return 0;
}

static int rtmp_payload_alloc(struct rtmp_parse_t* parser, size_t bytes)
{
	if (parser->capacity < bytes)
	{
		void* p = realloc(parser->payload, bytes + 1024);
		if (!p)
			return ENOMEM;

		parser->payload = p;
		parser->capacity = bytes + 1024;
	}

	return 0;
}

int rtmp_chunk_input(struct rtmp_t* rtmp, const uint8_t* data, size_t bytes)
{
	const static uint32_t s_header_size[] = { 11, 7, 3, 0 };

	size_t size, offset = 0;
	struct rtmp_parse_t parser;
	struct rtmp_chunk_header_t* header;
	//	parser.state = RTMP_STATE_INIT;

	while (offset < bytes)
	{
		switch (parser.state)
		{
		case RTMP_STATE_INIT:
			parser.header[0] = data[offset++];
			parser.bytes = 1;

			if (0 == (parser.header[0] & 0x3F))
				parser.basic_bytes = 2;
			else if (1 == (parser.header[0] & 0x3F))
				parser.basic_bytes = 3;
			else
				parser.basic_bytes = 1;

			parser.state = RTMP_STATE_BASIC_HEADER;
			break;

		case RTMP_STATE_BASIC_HEADER:
			assert(parser.bytes <= parser.basic_bytes);
			while (parser.bytes < parser.basic_bytes && offset < bytes)
			{
				parser.header[parser.bytes++] = data[offset++];
			}

			assert(parser.bytes <= parser.basic_bytes);
			if (parser.bytes >= parser.basic_bytes)
			{
				// parse basic header
				rtmp_read_basic_header(parser.header, &header->fmt, &header->cid);
				// load previous header

				parser.state = RTMP_STATE_MESSAGE_HEADER;
			}
			break;

		case RTMP_STATE_MESSAGE_HEADER:
			size = s_header_size[header->fmt] + parser.basic_bytes;
			assert(parser.bytes < size);
			while (parser.bytes < size && offset < bytes)
			{
				parser.header[parser.bytes++] = data[offset++];
			}

			assert(parser.bytes <= size);
			if (parser.bytes >= size)
			{
				// parse message header
				rtmp_read_message_header(parser.header + parser.basic_bytes, header);

				if (0 != rtmp_payload_alloc(&parser, header->length))
					return ENOMEM;
				parser.state = RTMP_STATE_EXTENDED_TIMESTAMP;
			}
			break;

		case RTMP_STATE_EXTENDED_TIMESTAMP:
			size = s_header_size[header->fmt] + parser.basic_bytes;
			if (header->timestamp >= 0xFFFFFF) size += 4;

			assert(parser.bytes < size);
			while (parser.bytes < size && offset < bytes)
			{
				parser.header[parser.bytes++] = data[offset++];
			}

			assert(parser.bytes <= size);
			if (parser.bytes >= size)
			{
				// parse extended timestamp
				rtmp_read_extended_timestamp(parser.header + s_header_size[header->fmt] + parser.basic_bytes, &header->timestamp);

				parser.state = RTMP_STATE_PAYLOAD;
			}
			break;

		case RTMP_STATE_PAYLOAD:
			assert(parser.payloadSize < header->length);
			size = MIN(rtmp->chunk_size - (parser.payloadSize % rtmp->chunk_size), header->length - parser.payloadSize);
			size = MIN(size, bytes - offset);
			memcpy(parser.payload + parser.payloadSize, data + offset, size);
			offset += size;

			if (parser.payloadSize >= header->length)
			{
				assert(parser.payloadSize == header->length);
				rtmp->onpacket(rtmp->param, header, parser.payload);
				parser.state = RTMP_STATE_INIT;
			}
			else if (0 == parser.payloadSize % rtmp->chunk_size)
			{
				// next chunk
				parser.state = RTMP_STATE_INIT;
			}
			break;
		}
	}

	return 0;
}
