#ifndef _rtp_packet_h_
#define _rtp_packet_h_

#include "rtp-header.h"
#include "rtcp-header.h"
#include "rtp-util.h"

typedef struct _rtp_packet_t
{
	rtp_header_t rtp;
	uint32_t csrc[16];
	void* extension; // extension(valid only if rtp.x = 1)
	size_t extlen; // extension length in bytes
	void* payload; // payload
	size_t payloadlen; // payload length in bytes
} rtp_packet_t;

static inline int rtp_packet_deserialize(rtp_packet_t *pkt, const void* data, size_t bytes)
{
	uint32_t i, v;
	size_t hdrlen;
	const unsigned char *ptr;

	assert(12 == sizeof(rtp_header_t));
	ptr = (const unsigned char *)data;
	if(bytes < 12) // RFC3550 5.1 RTP Fixed Header Fields(p12)
		return -1;

	memset(pkt, 0, sizeof(rtp_packet_t));

	// pkt header
	v = nbo_r32(ptr);
	pkt->rtp.v = RTP_V(v);
	pkt->rtp.p = RTP_P(v);
	pkt->rtp.x = RTP_X(v);
	pkt->rtp.cc = RTP_CC(v);
	pkt->rtp.m = RTP_M(v);
	pkt->rtp.pt = RTP_PT(v);
	pkt->rtp.seq = RTP_SEQ(v);
	pkt->rtp.timestamp = nbo_r32(ptr+4);
	pkt->rtp.ssrc = nbo_r32(ptr+8);

	assert(2 == pkt->rtp.v); // RTP version field must equal 2 (p66)
	hdrlen = 12/*sizeof(rtp_header_t)*/ + pkt->rtp.cc * 4;
	if(bytes < hdrlen + (pkt->rtp.x?4:0) + (pkt->rtp.p?1:0))
		return -1;

	// pkt contributing source
	for(i = 0; i < pkt->rtp.cc; i++)
	{
		pkt->csrc[i] = nbo_r32(ptr + 12 + i*4);
	}

	assert(bytes > hdrlen);
	pkt->payload = (unsigned char*)data + hdrlen;
	pkt->payloadlen = bytes - hdrlen;

	// pkt header extension
	if(1 == pkt->rtp.x)
	{
		unsigned char *rtpext = (unsigned char*)data + hdrlen;
		pkt->extension = rtpext + 4;
		pkt->extlen = nbo_r16(rtpext+2) * 4;
		assert(pkt->payloadlen >= pkt->extlen + 4);
		pkt->payload = (unsigned char*)pkt->payload + pkt->extlen + 4;
		pkt->payloadlen -= pkt->extlen + 4;
	}

	// padding
	if(1 == pkt->rtp.p)
	{
		unsigned char *padding = (unsigned char*)data + bytes - 1;
		assert(pkt->payloadlen >= *padding);
		pkt->payloadlen -= *padding;
	}

	return (pkt->payloadlen > bytes - hdrlen) ? -1 : 0;
}

#endif /* !_rtp_packet_h_ */
