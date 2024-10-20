#include "rtmp-chunk-header.h"
#include "rtmp-internal.h"
#include "rtmp-util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define MAX_CHECK_TYPE3_WITH_TIMESTAMP 7

// 5.3.1. Chunk Format (p11)
/* 3-bytes basic header + 11-bytes message header + 4-bytes extended timestamp */
//#define MAX_CHUNK_HEADER 18

static struct rtmp_packet_t* rtmp_packet_find(struct rtmp_t* rtmp, uint32_t cid)
{
	uint32_t i;
	struct rtmp_packet_t* pkt;

	// The protocol supports up to 65597 streams with IDs 3-65599
	assert(cid <= 65535 + 64 && cid >= 2 /* Protocol Control Messages */);
	for (i = 0; i < N_CHUNK_STREAM; i++)
	{
		pkt = rtmp->out_packets + ((i + cid) % N_CHUNK_STREAM);
		if (pkt->header.cid == cid)
			return pkt;
	}
	return NULL;
}

//static struct rtmp_packet_t* rtmp_packet_create(struct rtmp_t* rtmp, uint32_t cid)
//{
//	uint32_t i;
//	struct rtmp_packet_t* pkt;
//
//	// The protocol supports up to 65597 streams with IDs 3-65599
//	assert(cid <= 65535 + 64 && cid >= 2 /* Protocol Control Messages */);
//	assert(NULL == rtmp_packet_find(rtmp, cid));
//	for (i = 0; i < N_CHUNK_STREAM; i++)
//	{
//		pkt = rtmp->out_packets + ((i + cid) % N_CHUNK_STREAM);
//		if (0 == pkt->header.cid)
//			return pkt;
//	}
//	return NULL;
//}

static const struct rtmp_chunk_header_t* rtmp_chunk_header_zip(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header)
{
	struct rtmp_packet_t* pkt; // previous saved chunk header
	struct rtmp_chunk_header_t h;

	assert(0 != header->cid && 1 != header->cid);
	assert(RTMP_CHUNK_TYPE_0 == header->fmt || RTMP_CHUNK_TYPE_1 == header->fmt);
	
	memcpy(&h, header, sizeof(h));

	// find previous chunk header
	pkt = rtmp_packet_find(rtmp, h.cid);
	if (NULL == pkt)
	{
		//pkt = rtmp_packet_create(rtmp, h.cid);
		//if (NULL == pkt)
		//	return NULL; // too many chunk stream id
		assert(0);
		return NULL; // can't find chunk stream id 
	}

	h.fmt = RTMP_CHUNK_TYPE_0;
	if (RTMP_CHUNK_TYPE_0 != header->fmt /* enable compress */
		&& header->cid == pkt->header.cid /* not the first packet */
		&& header->timestamp >= pkt->clock /* timestamp wrap */
		&& header->timestamp - pkt->clock < 0xFFFFFF /* timestamp delta < 1 << 24 */
		&& header->stream_id == pkt->header.stream_id /* message stream id */)
	{
		h.fmt = RTMP_CHUNK_TYPE_1;
		h.timestamp -= pkt->clock; // timestamp delta

		if (header->type == pkt->header.type && header->length == pkt->header.length)
		{
			h.fmt = RTMP_CHUNK_TYPE_2;
			if (h.timestamp == pkt->header.timestamp)
				h.fmt = RTMP_CHUNK_TYPE_3;
		}
	}

	memcpy(&pkt->header, &h, sizeof(h));
	pkt->clock = header->timestamp; // save timestamp
	return &pkt->header;
}

int rtmp_chunk_write(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* h, const uint8_t* payload)
{
	int r = 0;
	uint8_t *p;
	uint32_t chunkSize, headerSize, payloadSize, N;
	const struct rtmp_chunk_header_t* header;

	// compression rtmp chunk header
	header = rtmp_chunk_header_zip(rtmp, h);
	if (!header || header->length >= 0xFFFFFF)
		return -EINVAL; // invalid length

	// alloc chunk header
	N = header->cid < 64 ? 1 : (header->cid < (64 + 256) ? 2 : 3) /*type-3 header*/ + header->timestamp >= 0xFFFFFF ? 4 : 0;
	if (MAX_CHUNK_HEADER + header->length / rtmp->out_chunk_size * N > rtmp->chunk_write_header.capacity)
	{
		p = realloc(rtmp->chunk_write_header.ptr == rtmp->chunk_write_header.bufer ? 0 : rtmp->chunk_write_header.ptr, MAX_CHUNK_HEADER + header->length / rtmp->out_chunk_size * MAX_CHECK_TYPE3_WITH_TIMESTAMP);
		if (!p)
			return -ENOMEM;
		rtmp->chunk_write_header.ptr = p;
		rtmp->chunk_write_header.capacity = MAX_CHUNK_HEADER + header->length / rtmp->out_chunk_size * MAX_CHECK_TYPE3_WITH_TIMESTAMP;
	}
	p = rtmp->chunk_write_header.ptr;

	payloadSize = header->length;
	headerSize = rtmp_chunk_basic_header_write(p, header->fmt, header->cid);
	headerSize += rtmp_chunk_message_header_write(p + headerSize, header);
	if(header->timestamp >= 0xFFFFFF)
		headerSize += rtmp_chunk_extended_timestamp_write(p + headerSize, header->timestamp);
	assert(headerSize < MAX_CHUNK_HEADER);

	while (payloadSize > 0 && 0 == r)
	{
		chunkSize = payloadSize < rtmp->out_chunk_size ? payloadSize : rtmp->out_chunk_size;
		r = rtmp->send(rtmp->param, p, headerSize, payload, chunkSize); // callback

		payload += chunkSize;
		payloadSize -= chunkSize;

		if (payloadSize > 0)
		{
			p += headerSize; // next chunk header
			assert(p + N <= rtmp->chunk_write_header.ptr + rtmp->chunk_write_header.capacity);
			headerSize = rtmp_chunk_basic_header_write(p, RTMP_CHUNK_TYPE_3, header->cid);
			if (header->timestamp >= 0xFFFFFF)
				headerSize += rtmp_chunk_extended_timestamp_write(p + headerSize, header->timestamp);
			assert(headerSize == N);
		}
	}

	return r;
}
