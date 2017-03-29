#include "rtmp-message.h"
#include "rtmp-internal.h"

#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>

static struct rtmp_chunk_header_t* rtmp_chunk_header_find(struct rtmp_t* rtmp, uint32_t id)
{
	uint32_t i, offset = id % N_CHUNK_STREAM;

	// The protocol supports up to 65597 streams with IDs 3-65599
	assert(id <= 65535 + 64 && id >= 2 /* Protocol Control Messages */);
	for (i = offset; i < offset + N_CHUNK_STREAM; i++)
	{
		if (rtmp->out_headers[i % N_CHUNK_STREAM].cid == id)
			return &rtmp->out_headers[i % N_CHUNK_STREAM];
	}
	return NULL;
}

static struct rtmp_chunk_header_t* rtmp_chunk_header_create(struct rtmp_t* rtmp, uint32_t id)
{
	uint32_t i;
	assert(NULL == rtmp_chunk_header_find(rtmp, id));

	assert(id <= 65535 + 64 && id >= 2 /* Protocol Control Messages */);
	for (i = 0; i < N_CHUNK_STREAM; i++)
	{
		if (0 == rtmp->out_headers[i].cid)
			return &rtmp->out_headers[i];
	}
	return NULL;
}

int rtmp_message_send(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* payload)
{
	uint32_t timestamp = 0;
	struct rtmp_chunk_header_t optHeader, *prevHeader;

	memcpy(&optHeader, header, sizeof(struct rtmp_chunk_header_t));

	prevHeader = rtmp_chunk_header_find(rtmp, header->cid);
	if (NULL == prevHeader)
	{
		prevHeader = rtmp_chunk_header_create(rtmp, header->cid);
		if (NULL == prevHeader)
			return ENOMEM;
	}
	else
	{
		if (RTMP_CHUNK_TYPE_0 != optHeader.fmt)
		{
			if (prevHeader->type == header->type
				&& prevHeader->length == header->length
				&& optHeader.fmt == RTMP_CHUNK_TYPE_1)
				optHeader.fmt = RTMP_CHUNK_TYPE_2;

			if (prevHeader->timestamp == header->timestamp
				&& optHeader.fmt == RTMP_CHUNK_TYPE_2)
				optHeader.fmt = RTMP_CHUNK_TYPE_3;

			timestamp = prevHeader->timestamp;
		}
	}

	memcpy(prevHeader, header, sizeof(struct rtmp_chunk_header_t)); // save header
	optHeader.timestamp -= timestamp; // timestamp delta
	return rtmp_chunk_send(rtmp, &optHeader, payload);
}
