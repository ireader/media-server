#include "rtp-payload-helper.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void* rtp_payload_helper_create(struct rtp_payload_t *handler, void* cbparam)
{
	struct rtp_payload_helper_t *helper;
	helper = (struct rtp_payload_helper_t *)calloc(1, sizeof(*helper));
	if (!helper)
		return NULL;

	memcpy(&helper->handler, handler, sizeof(helper->handler));
	helper->cbparam = cbparam;
	helper->flags = -1;
	return helper;
}

void rtp_payload_helper_destroy(void* p)
{
	struct rtp_payload_helper_t *helper;
	helper = (struct rtp_payload_helper_t *)p;

	if (helper->ptr)
		free(helper->ptr);
#if defined(_DEBUG) || defined(DEBUG)
	memset(helper, 0xCC, sizeof(*helper));
#endif
	free(helper);
}

int rtp_payload_check(struct rtp_payload_helper_t* helper, const struct rtp_packet_t* pkt)
{
	// first packet only
	if (-1 == helper->flags)
	{
		helper->flags = 0;
		helper->seq = (uint16_t)(pkt->rtp.seq - 1); // disable packet lost
		helper->timestamp = pkt->rtp.timestamp + 1; // flag for new frame
	}

	// check sequence number
	if ((uint16_t)pkt->rtp.seq != (uint16_t)(helper->seq + 1))
	{
		helper->size = 0;
		helper->lost = 1;
		helper->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;
		helper->timestamp = pkt->rtp.timestamp;
	}
	helper->seq = (uint16_t)pkt->rtp.seq;

	// check timestamp
	if (pkt->rtp.timestamp != helper->timestamp)
	{
		rtp_payload_onframe(helper);
	}
	helper->timestamp = pkt->rtp.timestamp;

	return 0;
}

int rtp_payload_write(struct rtp_payload_helper_t* helper, const struct rtp_packet_t* pkt)
{
	if (helper->size + pkt->payloadlen > helper->capacity)
	{
		void *ptr;
		int size;

		size = helper->size + pkt->payloadlen + 8000;
		ptr = realloc(helper->ptr, size);
		if (!ptr)
		{
			helper->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;
			helper->lost = 1;
			helper->size = 0;
			return -ENOMEM;
		}

		helper->ptr = (uint8_t*)ptr;
		helper->capacity = size;
	}

	assert(helper->capacity >= helper->size + pkt->payloadlen);
	memcpy(helper->ptr + helper->size, pkt->payload, pkt->payloadlen);
	helper->size += pkt->payloadlen;
	return 0;
}

int rtp_payload_onframe(struct rtp_payload_helper_t *helper)
{
	if (helper->size > 0)
	{
		// previous packet done
		assert(!helper->lost);
		helper->handler.packet(helper->cbparam, helper->ptr, helper->size, helper->timestamp, helper->flags);
		helper->flags &= ~RTP_PAYLOAD_FLAG_PACKET_LOST; // clear packet lost flag	
	}

	// new frame start
	helper->lost = 0;
	helper->size = 0;
	return 0;
}
