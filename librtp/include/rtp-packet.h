#ifndef _rtp_packet_h_
#define _rtp_packet_h_

#include "rtp.h"

typedef struct _rtp_packet_t
{
	rtp_header_t rtp;
	uint32_t csrc[16];
	void* extension; // extension(valid only if rtp.x = 1)
	size_t extlen; // extension length in bytes
	void* payload; // payload
	size_t payloadlen; // payload length in bytes
} rtp_packet_t;

inline int rtp_deserialize(rtp_packet_t *pkt, const void* data, size_t bytes)
{
	uint32_t i, v;
	size_t hdrlen;
	const unsigned int *ptr;

	ptr = (const unsigned int *)data;
	if(bytes < sizeof(rtp_header_t))
		return -1;

	memset(pkt, 0, sizeof(rtp_packet_t));

	// pkt header
	v = ntohl(ptr[0]);
	pkt->rtp.v = RTP_V(v);
	pkt->rtp.p = RTP_P(v);
	pkt->rtp.x = RTP_X(v);
	pkt->rtp.cc = RTP_CC(v);
	pkt->rtp.m = RTP_M(v);
	pkt->rtp.pt = RTP_PT(v);
	pkt->rtp.seq = RTP_SEQ(v);
	pkt->rtp.timestamp = ntohl(ptr[1]);
	pkt->rtp.ssrc = ntohl(ptr[2]);

	hdrlen = sizeof(rtp_header_t) + pkt->rtp.cc * 4;
	if(bytes < hdrlen + (pkt->rtp.x?4:0) + (pkt->rtp.p?1:0))
		return -1;

	// pkt contributing source
	for(i = 0; i < pkt->rtp.cc; i++)
	{
		pkt->csrc[i] = ntohl(ptr[3+i]);
	}

	assert(bytes > hdrlen);
	pkt->payload = (unsigned char*)data + hdrlen;
	pkt->payloadlen = bytes - hdrlen;

	// pkt header extension
	if(1 == pkt->rtp.x)
	{
		unsigned char *rtpext = (unsigned char*)data + hdrlen + 4;
		unsigned short extlen = ((unsigned short*)rtpext)[1];
		pkt->extension = rtpext;
		pkt->extlen = ntohs(extlen) * 4;
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
