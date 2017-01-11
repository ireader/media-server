#ifndef _rtmp_internal_h_
#define _rtmp_internal_h_

#include "rtmp-message.h"

#define N_CHUNK_STREAM 10 // maximum chunk stream count

struct rtmp_t
{
	uint32_t chunk_size;

	int transaction_id;

	// saved chunk header
	struct rtmp_chunk_header_t headers[N_CHUNK_STREAM];

	void* param;

	int (*send)(void* param, const uint8_t* header, uint32_t headerBytes, const uint8_t* payload, uint32_t payloadBytes);
	int (*onrecv)(void* param);
};

int rtmp_chunk_send(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* payload);

#endif /* !_rtmp_internal_h_ */
