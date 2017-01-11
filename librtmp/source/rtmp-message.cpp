#include "rtmp-message.h"
#include "rtmp-internal.h"

#include <stdlib.h>
#include <memory.h>
#include <assert.h>

static struct rtmp_chunk_header_t* rtmp_chunk_header_find(struct rtmp_t* rtmp, uint32_t id)
{
	uint32_t i, offset = id % N_CHUNK_STREAM;

	// The protocol supports up to 65597 streams with IDs 3-65599
	assert(id < 65535 + 64 && id >= 2 /* Protocol Control Messages */);
	for (i = offset; i < offset + N_CHUNK_STREAM; i++)
	{
		if (rtmp->headers[i % N_CHUNK_STREAM].cid == id)
			return &rtmp->headers[i % N_CHUNK_STREAM];
	}
	return NULL;
}

int rtmp_message_send(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* payload)
{
	struct rtmp_chunk_header_t *prevHeader, *chunkHeader;

	memcpy(chunkHeader, header, sizeof(*chunkHeader));
	prevHeader = rtmp_chunk_header_find(rtmp, header->cid);
	if (!prevHeader && header->fmt != RTMP_CHUNK_TYPE_0)
	{
		chunkHeader->fmt = RTMP_CHUNK_TYPE_0;
	}
	else if (prevHeader && chunkHeader->fmt != RTMP_CHUNK_TYPE_0)
	{
		if (prevHeader->type == header->type
			&& prevHeader->length == header->length
			&& header->fmt == RTMP_CHUNK_TYPE_1)
			chunkHeader->fmt = RTMP_CHUNK_TYPE_2;

		if(prevHeader->timestamp == header->timestamp
			&& header->fmt == RTMP_CHUNK_TYPE_2)
			chunkHeader->fmt = RTMP_CHUNK_TYPE_3;
	}

	return rtmp_chunk_send(rtmp, chunkHeader, payload);
}
