#include "rtmp-internal.h"
#include "rtmp-util.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))

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

static int rtmp_packet_alloc(struct rtmp_packet_t* packet, size_t bytes)
{
	if (packet->capacity < bytes)
	{
		void* p = realloc(packet->payload, bytes + 1024);
		if (!p)
			return ENOMEM;

		packet->payload = p;
		packet->capacity = bytes + 1024;
	}

	return 0;
}

static struct rtmp_packet_t* rtmp_packet_load(struct rtmp_t* rtmp, uint32_t id)
{
	uint32_t i, offset = id % N_CHUNK_STREAM;

	// The protocol supports up to 65597 streams with IDs 3-65599
	assert(id <= 65535 + 64 && id >= 2 /* Protocol Control Messages */);
	for (i = offset; i < offset + N_CHUNK_STREAM; i++)
	{
		if (id == rtmp->in_packets[i % N_CHUNK_STREAM].header.cid)
			return &rtmp->in_packets[i % N_CHUNK_STREAM];
	}

	for (i = 0; i < N_CHUNK_STREAM; i++)
	{
		if (0 == rtmp->in_packets[i].header.cid)
		{
			rtmp->in_packets[i].header.cid = id;
			return &rtmp->in_packets[i];
		}
	}
	return NULL;
}

static struct rtmp_packet_t* rtmp_packet_load_by_paser(struct rtmp_t* rtmp, struct rtmp_parser_t* parser)
{
	uint8_t fmt = 0;
	uint32_t cid = 0;
	struct rtmp_packet_t* pakcet = NULL;

	// parse basic header
	rtmp_read_basic_header(parser->buffer, &fmt, &cid);

	// load previous header
	pakcet = rtmp_packet_load(rtmp, cid);
	if (NULL != pakcet)
	{
		pakcet->header.fmt = fmt;
	}
	return pakcet;
}

static struct rtmp_packet_t* rtmp_packet_parse(struct rtmp_t* rtmp, struct rtmp_parser_t* parser)
{
	struct rtmp_packet_t* pakcet = NULL;

	// load previous header
	pakcet = rtmp_packet_load_by_paser(rtmp, parser);
	if (NULL == pakcet)
		return NULL;

	// parse message header
	rtmp_read_message_header(parser->buffer + parser->basic_bytes, &pakcet->header);

	// alloc memory
	if (0 != rtmp_packet_alloc(pakcet, pakcet->header.length))
		return NULL;

	return pakcet;
}

int rtmp_chunk_input(struct rtmp_t* rtmp, const uint8_t* data, size_t bytes)
{
	const static uint32_t s_header_size[] = { 11, 7, 3, 0 };

	size_t size, offset = 0;
	struct rtmp_packet_t* packet = NULL;
	struct rtmp_parser_t* parser = &rtmp->parser;
	struct rtmp_chunk_header_t header;

	while (offset < bytes)
	{
		switch (parser->state)
		{
		case RTMP_STATE_INIT:
			parser->buffer[0] = data[offset++];
			parser->bytes = 1;

			if (0 == (parser->buffer[0] & 0x3F))
				parser->basic_bytes = 2;
			else if (1 == (parser->buffer[0] & 0x3F))
				parser->basic_bytes = 3;
			else
				parser->basic_bytes = 1;

			parser->state = RTMP_STATE_BASIC_HEADER;
			break;

		case RTMP_STATE_BASIC_HEADER:
			assert(parser->bytes <= parser->basic_bytes);
			while (parser->bytes < parser->basic_bytes && offset < bytes)
			{
				parser->buffer[parser->bytes++] = data[offset++];
			}

			assert(parser->bytes <= parser->basic_bytes);
			if (parser->bytes >= parser->basic_bytes)
			{
				parser->state = RTMP_STATE_MESSAGE_HEADER;
			}
			break;

		case RTMP_STATE_MESSAGE_HEADER:
			size = s_header_size[parser->buffer[0] >> 6] + parser->basic_bytes;
			assert(parser->bytes <= size);
			while (parser->bytes < size && offset < bytes)
			{
				parser->buffer[parser->bytes++] = data[offset++];
			}

			assert(parser->bytes <= size);
			if (parser->bytes >= size)
			{
				parser->state = RTMP_STATE_EXTENDED_TIMESTAMP;
			}
			break;

		case RTMP_STATE_EXTENDED_TIMESTAMP:
			// new packet
			packet = rtmp_packet_parse(rtmp, parser);
			if (NULL == packet) return ENOMEM;

			size = s_header_size[parser->buffer[0] >> 6] + parser->basic_bytes;
			if (packet->header.timestamp >= 0xFFFFFF) size += 4;

			assert(parser->bytes <= size);
			while (parser->bytes < size && offset < bytes)
			{
				parser->buffer[parser->bytes++] = data[offset++];
			}

			assert(parser->bytes <= size);
			if (parser->bytes >= size)
			{
				// parse extended timestamp
				rtmp_read_extended_timestamp(parser->buffer + s_header_size[parser->buffer[0] >> 6] + parser->basic_bytes, &packet->header.timestamp);

				if (RTMP_CHUNK_TYPE_0 == packet->header.fmt)
					packet->timestamp = 0; // clear

				parser->state = RTMP_STATE_PAYLOAD;
			}
			break;

		case RTMP_STATE_PAYLOAD:
			if (NULL == packet) // continue chunk stream
			{
				packet = rtmp_packet_load_by_paser(rtmp, parser); // load previous header
				if (NULL == packet)
					return ENOMEM;
			}

			assert(packet->bytes < packet->header.length);
			assert(packet->capacity >= packet->header.length);
			size = MIN(rtmp->in_chunk_size - (packet->bytes % rtmp->in_chunk_size), packet->header.length - packet->bytes);
			size = MIN(size, bytes - offset);
			memcpy(packet->payload + packet->bytes, data + offset, size);
			packet->bytes += size;
			offset += size;

			if (packet->bytes >= packet->header.length)
			{
				assert(packet->bytes == packet->header.length);
				packet->timestamp += packet->header.timestamp; // +timestamp delta
				packet->bytes = 0; // reset packet buffer
				parser->state = RTMP_STATE_INIT; // reset parser state

				memcpy(&header, &packet->header, sizeof(header));
				header.timestamp = packet->timestamp;
				rtmp_handler(rtmp, &header, packet->payload);

				packet = NULL; // read next packet
			}
			else if (0 == packet->bytes % rtmp->in_chunk_size)
			{
				// next chunk
				parser->state = RTMP_STATE_INIT;
			}
			else
			{
				assert(offset == bytes);
			}
			break;

		default:
			assert(0);
			break;
		}
	}

	return 0;
}
