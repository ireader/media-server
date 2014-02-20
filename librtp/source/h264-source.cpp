#include "cstringext.h"
#include "sys/sock.h"
#include "rtp-packet.h"
#include "rtp-avp-udp.h"
#include "h264-source.h"
#include "rtp-queue.h"
#include "rtp.h"

#define H264_NAL(v)	(v & 0x1F)
#define FU_START(v) (v & 0x80)
#define FU_END(v)	(v & 0x40)
#define FU_NAL(v)	(v & 0x1F)

struct h264_source
{
	OnH264 cb;
	void* param;

	void* queue;

	void* ptr;
	int size, capacity;
};

void* h264_source_create(void* queue, OnH264 cb, void* param)
{
	struct h264_source *h264;
	h264 = (struct h264_source *)malloc(sizeof(struct h264_source));
	if(!h264)
		return NULL;

	memset(h264, 0, sizeof(struct h264_source));
	h264->cb = cb;
	h264->param = param;
	h264->queue = queue;
	return h264;
}

int h264_source_destroy(void* h264)
{
	struct h264_source *o;
	o = (struct h264_source *)h264;

	free(0);
	return 0;
}

static int h264_stap_a_parse(struct h264_source *h264, const void* data, int bytes)
{
	// 5.7.1. Single-Time Aggregation Packet (STAP)
	unsigned char stapnal;
	unsigned short nallen;
	const unsigned char* ptr;
	ptr = (const unsigned char*)data;
	stapnal = ptr[0];

	++ptr;
	for(bytes -= 1; bytes > 2; bytes -= nallen + 2)
	{
		unsigned char nal;
		unsigned short nallen;
		const unsigned short *p;
		p = (const unsigned short *)ptr;

		nallen = ntohs(p[0]);
		if(nallen > bytes - 2)
		{
			assert(0);
			return -1; // error
		}

		ptr = (const unsigned char*)(p + 1);
		nal = ptr[0];
		assert(H264_NAL(nal) > 0 && H264_NAL(nal) < 24);

		h264->cb(h264->param, nal, ptr+1, nallen-1);

		ptr = ptr + nallen; // next NALU
	}

	return 0;
}

static int h264_stap_b_parse(struct h264_source *h264, const void* data, int bytes)
{
	// 5.7.1. Single-Time Aggregation Packet (STAP)
	unsigned char stapnal;
	unsigned short don;
	unsigned short nallen;
	const unsigned char* ptr;
	ptr = (const unsigned char*)data;

	stapnal = ptr[0];
	don = ntohs(*(const unsigned short *)(ptr + 1));

	ptr += 3;
	for(bytes -= 1; bytes > 2; bytes -= nallen + 2)
	{
		unsigned char nal;
		unsigned short nallen;
		const unsigned short *p;
		p = (const unsigned short *)ptr;

		nallen = ntohs(p[0]);
		if(nallen > bytes - 2)
		{
			assert(0);
			return -1; // error
		}

		ptr = (const unsigned char*)(p + 1);
		nal = ptr[0];
		assert(H264_NAL(nal) > 0 && H264_NAL(nal) < 24);
		h264->cb(h264->param, nal, ptr+1, nallen-1);

		ptr = ptr + nallen; // next NALU
	}

	return 0;
}

static int h264_mtap16_parse(struct h264_source *h264, const void* data, int bytes)
{
	// 5.7.2. Multi-Time Aggregation Packets (MTAPs)
	unsigned char mtapnal;
	unsigned short donb;
	unsigned short nallen;
	const unsigned char* ptr;
	ptr = (const unsigned char*)data;

	mtapnal = ptr[0];
	donb = ntohs(*(const unsigned short *)(ptr + 1));

	ptr += 3;
	for(bytes -= 1; bytes > 2; bytes -= nallen + 2)
	{
		unsigned char nal, dond;
		unsigned short timestamp;
		const unsigned short *p;
		p = (const unsigned short *)ptr;

		nallen = ntohs(p[0]);
		if(nallen > bytes - 2)
		{
			assert(0);
			return -1; // error
		}

		assert(nallen > 4);
		ptr = (const unsigned char*)(p + 1);
		dond = ptr[0];
		timestamp = ntohs(*(unsigned short *)(ptr + 1));

		nal = ptr[3];
		assert(H264_NAL(nal) > 0 && H264_NAL(nal) < 24);
		h264->cb(h264->param, nal, ptr+4, nallen-4);

		ptr = ptr + nallen; // next NALU
	}

	return 0;
}

static int h264_mtap24_parse(struct h264_source *h264, const void* data, int bytes)
{
	// 5.7.2. Multi-Time Aggregation Packets (MTAPs)
	unsigned char mtapnal;
	unsigned short donb;
	unsigned short nallen;
	const unsigned char* ptr;
	ptr = (const unsigned char*)data;

	mtapnal = ptr[0];
	donb = ntohs(*(const unsigned short *)(ptr + 1));

	ptr += 3;
	for(bytes -= 1; bytes > 2; bytes -= nallen + 2)
	{
		unsigned char nal, dond;
		unsigned int timestamp;
		const unsigned short *p;
		p = (const unsigned short *)ptr;

		nallen = ntohs(p[0]);
		if(nallen > bytes - 2)
		{
			assert(0);
			return -1; // error
		}

		assert(nallen > 5);
		ptr = (const unsigned char*)(p + 1);
		dond = ptr[0];
		timestamp = (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];

		nal = ptr[4];
		assert(H264_NAL(nal) > 0 && H264_NAL(nal) < 24);
		h264->cb(h264->param, nal, ptr+5, nallen-5);

		ptr = ptr + nallen; // next NALU
	}

	return 0;
}

static int h264_fu_a_parse(struct h264_source *h264, const void* data, int bytes)
{
	unsigned char fuindicator, fuheader;
	unsigned short nallen;
	const unsigned char* ptr;
	ptr = (const unsigned char*)data;

	fuindicator = ptr[0];
	fuheader = ptr[1];

	if(FU_START(fuheader))
	{
		assert(0 == h264->size);
		h264->size = 0;
	}
	else
	{
		assert(0 < h264->size);
	}

	if(h264->size + bytes - 2 > h264->capacity)
	{
		int size = h264->size + bytes * 2;
		h264->ptr = realloc(h264->ptr, size);
		if(!h264->ptr)
		{
			h264->capacity = 0;
			h264->size = 0;
			return -1;
		}
		h264->capacity = size;
	}

	memmove((char*)h264->ptr + h264->size, ptr + 2, bytes - 2);
	h264->size += bytes - 2;

	if(FU_END(fuheader))
	{
		unsigned char nal;
		nal = (fuindicator & 0xE0) | (fuheader & 0x1F);
		assert(H264_NAL(nal) > 0 && H264_NAL(nal) < 24);
		h264->cb(h264->param, nal, h264->ptr, h264->size);
		h264->size = 0; // reset
	}

	return 0;
}

static int h264_fu_b_parse(struct h264_source *h264, const void* data, int bytes)
{
	unsigned char fuindicator, fuheader;
	unsigned short don;
	unsigned short nallen;
	const unsigned char* ptr;
	ptr = (const unsigned char*)data;

	fuindicator = ptr[0];
	fuheader = ptr[1];
	don = ntohs(*(unsigned short *)(ptr + 2));

	if(FU_START(fuheader))
	{
		assert(0 == h264->size);
		h264->size = 0;
	}
	else
	{
		assert(0 < h264->size);
	}

	if(h264->size + bytes - 4 > h264->capacity)
	{
		int size = h264->size + bytes * 2;
		h264->ptr = realloc(h264->ptr, size);
		if(!h264->ptr)
		{
			h264->capacity = 0;
			h264->size = 0;
			return -1;
		}
		h264->capacity = size;
	}

	memmove((char*)h264->ptr + h264->size, ptr + 4, bytes - 4);
	h264->size += bytes - 4;

	if(FU_END(fuheader))
	{
		unsigned char nal;
		nal = (fuindicator & 0xE0) | (fuheader & 0x1F);
		assert(H264_NAL(nal) > 0 && H264_NAL(nal) < 24);
		h264->cb(h264->param, nal, h264->ptr, h264->size);
		h264->size = 0; // reset
	}

	return 0;
}

int h264_source_process(void* param)
{
	void *rtp;
	int lost, len;
	rtp_packet_t pkt;
	unsigned char *ptr, nal;
	struct h264_source *h264;

	h264 = (struct h264_source *)param;
	if(0 != rtp_queue_read(h264->queue, &rtp, &len, &lost))
		return 0;

	if(lost)
	{
		// packet lost
		h264->size = 0; // clear fu-a/b flags
	}

	if(0 != rtp_deserialize(&pkt, rtp, len))
	{
		rtp_queue_free(h264->queue, rtp);
		return -1;
	}

	ptr = (unsigned char *)pkt.payload;
	nal = ptr[0];

	switch(nal & 0x1F)
	{
	case 0: // reserved
	case 31: // reserved
		assert(0);
		break;

	case 24: // STAP-A
		h264_stap_a_parse(h264, pkt.payload, pkt.payloadlen);
		break;
	case 25: // STAP-B
		h264_stap_b_parse(h264, pkt.payload, pkt.payloadlen);
		break;
	case 26: // MTAP16
		h264_mtap16_parse(h264, pkt.payload, pkt.payloadlen);
		break;
	case 27: // MTAP24
		h264_mtap24_parse(h264, pkt.payload, pkt.payloadlen);
		break;
	case 28: // FU-A
		h264_fu_a_parse(h264, pkt.payload, pkt.payloadlen);
		break;
	case 29: // FU-B
		h264_fu_b_parse(h264, pkt.payload, pkt.payloadlen);
		break;

	default: // 1-23 NAL unit
		h264->cb(h264->param, nal, ptr + 1, pkt.payloadlen-1);
		break;
	}

	rtp_queue_free(h264->queue, rtp);
	return 1;
}
