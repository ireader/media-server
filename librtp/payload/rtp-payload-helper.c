#include "rtp-payload-helper.h"
#include "rtp-param.h"
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
	helper->maxsize = RTP_PAYLOAD_MAX_SIZE;
	helper->cbparam = cbparam;
	helper->__flags = -1;
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
    int lost; // next frame lost packet flags
    
	// first packet only
	if (-1 == helper->__flags)
	{
        // TODO: first packet lost ???
		helper->__flags = 0;
		helper->seq = (uint16_t)(pkt->rtp.seq - 1); // disable packet lost
		helper->timestamp = pkt->rtp.timestamp + 1; // flag for new frame
	}
    
    lost = 0;
	// check sequence number
	if ((uint16_t)pkt->rtp.seq != (uint16_t)(helper->seq + 1))
	{
        lost = 1;
		//helper->size = 0;
		helper->lost = 1;
        //helper->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;
		//helper->timestamp = pkt->rtp.timestamp;
	}
	helper->seq = (uint16_t)pkt->rtp.seq;

	// check timestamp
	if (pkt->rtp.timestamp != helper->timestamp)
    {
        rtp_payload_onframe(helper);
        
        // lost:
        // 0 - packet lost before timestamp change
        // 1 - packet lost on timestamp changed, can't known losted packet is at old packet tail or new packet start, so two packets mark as packet lost
        if(0 != lost)
            helper->lost = lost;
    }
    
	helper->timestamp = pkt->rtp.timestamp;

	return 0;
}

int rtp_payload_write(struct rtp_payload_helper_t* helper, const struct rtp_packet_t* pkt)
{
	int size;
	size = helper->size + pkt->payloadlen;
	if (size > helper->maxsize || size < 0)
		return -EINVAL;

	if (size > helper->capacity)
	{
		void *ptr;

		size += size / 4 > 16000 ? size / 4 : 16000;
		ptr = realloc(helper->ptr, size);
		if (!ptr)
		{
            //helper->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;
			helper->lost = 1;
			//helper->size = 0;
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
	int r;
	r = 0;

    if (helper->size > 0
#if !defined(RTP_ENABLE_COURRUPT_PACKET)
        && 0 == helper->lost
#endif
        )
	{
		// previous packet done
        r = helper->handler.packet(helper->cbparam, helper->ptr, helper->size, helper->timestamp, helper->__flags | (helper->lost ? RTP_PAYLOAD_FLAG_PACKET_CORRUPT : 0));
        
        // RTP_PAYLOAD_FLAG_PACKET_LOST: miss
        helper->__flags &= ~RTP_PAYLOAD_FLAG_PACKET_LOST; // clear packet lost flag
	}
    
    // set packet lost flag on next frame
    if(helper->lost)
        helper->__flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;

	// new frame start
    helper->lost = 0;
	helper->size = 0;
	return r;
}
