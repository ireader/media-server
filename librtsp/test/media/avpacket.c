#include "avpacket.h"
#include "avbuffer.h"
#include <string.h>
#include <assert.h>

struct avpacket_t* avpacket_alloc(int size)
{
	struct avbuffer_t* buf;
	struct avpacket_t* pkt;
	buf = avbuffer_alloc(size + sizeof(struct avpacket_t));
	if (buf)
	{
		pkt = (struct avpacket_t*)buf->data;
		memset(pkt, 0, sizeof(struct avpacket_t));
		pkt->data = (uint8_t*)(pkt + 1);
		pkt->size = size;
		pkt->opaque = buf;
		return pkt;
	}
	return NULL;
}

int32_t avpacket_addref(struct avpacket_t* pkt)
{
	struct avbuffer_t* buf;
	if (NULL == pkt || NULL == pkt->opaque)
		return -1;

	buf = (struct avbuffer_t*)pkt->opaque;
	return avbuffer_addref(buf);
}

int32_t avpacket_release(struct avpacket_t* pkt)
{
	struct avbuffer_t* buf;
	if (NULL == pkt || NULL == pkt->opaque)
		return -1;

	buf = (struct avbuffer_t*)pkt->opaque;
	return avbuffer_release(buf);
}
