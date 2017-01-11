#include "rtmp-internal.h"
#include "rtmp-util.h"

// 5.3.1. Chunk Format (p11)
/* 3-bytes basic header + 11-bytes message header + 4-bytes extended timestamp */
#define MAX_CHUNK_HEADER 18

static int rtmp_write_basic_header(uint8_t* out, uint8_t fmt, uint32_t id)
{
	if (id > 64 + 255)
	{
		*out++ = (fmt << 6) | 1;
		*out++ = (uint8_t)((id - 64) & 0xFF);
		*out++ = (uint8_t)(((id - 64) >> 8) & 0xFF);
		return 2;
	}
	else if (id > 64)
	{
		*out++ = (fmt << 6) | 0;
		*out++ = (uint8_t)(id - 64);
		return 1;
	}
	else
	{
		*out++ = (fmt << 6) | (uint8_t)id;
		return 0;
	}
}

static int rtmp_write_message_header(uint8_t* out, const struct rtmp_chunk_header_t* header)
{
	const static int s_header_size[] = { 11, 7, 3, 0 };

	// timestamp / delta
	if (header->fmt <= RTMP_CHUNK_TYPE_2)
	{
		be_write_uint24(out, header->timestamp >= 0xFFFFFF ? 0xFFFFFF : header->timestamp);
		out += 3;
	}

	// message length + type
	if (header->fmt <= RTMP_CHUNK_TYPE_1)
	{
		be_write_uint24(out, header->length);
		out[3] = header->type;
		out += 4;
	}

	// message stream id
	if (header->fmt == RTMP_CHUNK_TYPE_0)
	{
		le_write_uint32(out, header->stream_id);
		out += 4;
	}

	return s_header_size[header->fmt % 4];
}

static int rtmp_write_extended_timestamp(uint8_t* out, uint32_t timestamp)
{
	if (timestamp >= 0xFFFFFF)
	{
		// extended timestamp
		be_write_uint32(out, timestamp);
		return 4;
	}
	return 0;
}

int rtmp_chunk_send(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* payload)
{
	uint8_t p[MAX_CHUNK_HEADER];
	uint32_t chunkSize, headerSize, payloadSize;

	if (header->length >= 0xFFFFFF)
		return -1; // invalid length

	payloadSize = header->length;
	headerSize = rtmp_write_basic_header(p, header->fmt, header->cid);
	headerSize += rtmp_write_message_header(p + headerSize, header);
	headerSize += rtmp_write_extended_timestamp(p + headerSize, header->timestamp);

	do
	{
		chunkSize = payloadSize < rtmp->chunk_size ? payloadSize : rtmp->chunk_size;
		rtmp->send(rtmp->param, p, headerSize, payload, chunkSize); // callback

		payload += chunkSize;
		payloadSize -= chunkSize;
		headerSize = 0;

		if (payloadSize > 0)
		{
			headerSize = rtmp_write_basic_header(p, header->fmt, header->cid);
			headerSize += rtmp_write_extended_timestamp(p + headerSize, header->timestamp);
		}
	} while (headerSize > 0);

	return 0;
}
