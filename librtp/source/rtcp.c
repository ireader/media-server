#include "cstringext.h"
#include "rtp-transport.h"
#include <stdio.h>

static struct rtp_source* rtp_source_fetch(struct rtp_context *ctx, unsigned int ssrc)
{
	struct rtp_source *p;
	p = rtp_source_list_find(ctx->members, ssrc);
	if(!p)
	{
		// exist in sender list?
		assert(!rtp_source_list_find(ctx->senders, ssrc));

		p = rtp_source_create(ssrc);
		if(p)
		{
			// update members list
			rtp_source_list_add(ctx->members, p);
		}
	}
	return p;
}

static struct rtp_source* rtp_sender_fetch(struct rtp_context *ctx, unsigned int ssrc)
{
	struct rtp_source *p;
	p = rtp_source_list_find(ctx->senders, ssrc);
	if(!p)
	{
		p = rtp_source_fetch(ctx, ssrc);
		if(p)
		{
			// update senders list
			rtp_source_list_add(ctx->senders, p);
		}
	}
	return p;
}

static void rtcp_input_sr(struct rtp_context *ctx, rtcp_header_t *header, const void* data)
{
	// RFC3550 6.4.1 SR: Sender Report RTCP Packet
	unsigned int i;
	rtcp_sr_t *sr;
	rtcp_rb_t *rb;
	struct rtp_source *sender;

	assert(header->length >= sizeof(rtcp_sr_t) + 4);
	sr = (rtcp_sr_t*)data;
	sr->ssrc = ntohl(sr->ssrc);
	if(!(sender = rtp_sender_fetch(sr->ssrc)))
		return; // error

	// update sender information
	sender->ntpts = (((time64_t)ntohl(sr->ntpts0))<<32) | ntohl(sr->ntpts1);
	sender->rtpts = ntohl(sr->rtpts);
	sender->spc = ntohl(sr->spc);
	sender->soc = ntohl(sr->soc);

	// ignore
	rb = (rtcp_rb_t *)(sr + 1);
	for(i = 0; i < header->rc; i++, rb++)
	{
		
	}
}

static void rtcp_input_rr(struct rtp_context *ctx, rtcp_header_t *header, const void* data)
{
	// RFC3550 6.4.2 RR: Receiver Report RTCP Packet
	unsigned int i;
	rtcp_rr_t *rr;
	rtcp_rb_t *rb;

	assert(header->length >= sizeof(rtcp_rr_t) + 4);
	rr = (rtcp_rr_t*)data;
	rr->ssrc = ntohl(rr->ssrc);

	// ignore
	rb = (rtcp_rb_t *)(rr + 1);
	for(i = 0; i < header->rc; i++, rb++)
	{

	}
}

static void rtcp_input_sdes(struct rtp_context *ctx, rtcp_header_t *header, const void* data)
{
	// RFC3550 6.5 SDES: Source Description RTCP Packet
	unsigned int i;
	unsigned int ssrc;
	rtcp_sdes_item_t *sdes;
	struct rtp_source *source;
	const unsigned char* p;

	assert(header->length >= header->rc * 4 + 4);
	p = (const unsigned char*)data;
	for(i = 0; i < header->rc; i++)
	{
		ssrc = ntohl((unsigned long*)p);
		if(!(source = rtp_source_fetch(ctx, ssrc)))
			continue;

		sdes = (rtcp_sdes_item_t*)(p + 4);
		while(RTCP_SDES_END != sdes->pt)
		{
			switch(sdes->pt)
			{
			case RTCP_SDES_CNAME:
				rtp_source_setcname(source, sdes->data, sdes->len);
				break;

			case RTCP_SDES_NAME:
				rtp_source_setname(source, sdes->data, sdes->len);
				break;

			case RTCP_SDES_EMAIL:
				rtp_source_setemail(source, sdes->data, sdes->len);
				break;

			case RTCP_SDES_PHONE:
				rtp_source_setphone(source, sdes->data, sdes->len);
				break;

			case RTCP_SDES_LOC:
				rtp_source_setloc(source, sdes->data, sdes->len);
				break;

			case RTCP_SDES_TOOL:
				rtp_source_settool(source, sdes->data, sdes->len);
				break;

			case RTCP_SDES_NOTE:
				rtp_source_setnote(source, sdes->data, sdes->len);
				break;

			case RTCP_SDES_PRIVATE:
				break;

			default:
				assert(0);
			}

			// RFC3550 6.5 SDES: Source Description RTCP Packet
			// Items are contiguous, i.e., items are not individually padded to a 32-bit boundary. 
			// Text is not null terminated because some multi-octet encodings include null octets.
			assert(1 == sizeof(sdes->data[0])); // unsigned char
			sdes = (rtcp_sdes_item_t*)(sdes->data + sdes->len);
		}

		// RFC3550 6.5 SDES: Source Description RTCP Packet
		// The list of items in each chunk must be terminated by one or more null octets,
		// the first of which is interpreted as an item type of zero to denote the end of the list.
		// No length octet follows the null item type octet, 
		// but additional null octets must be included if needed to pad until the next 32-bit boundary.
		// offset sizeof(SSRC) + sizeof(chunk type) + sizeof(chunk length)
		p += (((unsigned char*)sdes - p) + 3) / 4 * 4;
	}
}

static void rtcp_input_bye(struct rtp_context *ctx, rtcp_header_t *header, const void* data)
{
	// RFC3550 6.6 BYE: Goodbye RTCP Packet
	unsigned int i;
	const unsigned int *ssrc;
	unsigned char len; // reason length
	const char *reason;

	assert(header->length >= header->rc * 4 + 4);
	ssrc = (const unsigned int*)data;
	for(i = 0; i < header->rc; i++, ssrc++)
	{
		rtp_source_list_delete(ctx->members, ntohl(*ssrc));
		rtp_source_list_delete(ctx->senders, ntohl(*ssrc));
	}

	if(header->length > header->rc * 4 + 4)
	{
		len = *((unsigned char *)data + 4 + header->rc * 4);
		reason = (const char *)data + 4 + header->rc * 4 + 1;
	}
}

static void rtcp_input_app(struct rtp_context *ctx, rtcp_header_t *header, const void* data)
{
	// RFC3550 6.7 APP: Application-Defined RTCP Packet
}

static int rtcp_parse(struct rtp_context *ctx, const void* data, int bytes)
{
	unsigned long rtcphd;
	rtcp_header_t header;

	if(bytes >= sizeof(rtcphd))
	{
		rtcphd = ntohl(*(unsigned long*)data);

		header.v = RTCP_V(rtcphd);
		header.p = RTCP_P(rtcphd);
		header.rc = RTCP_RC(rtcphd);
		header.pt = RTCP_PT(rtcphd);
		header.length = (RTCP_LEN(rtcphd) + 1) * 4;
		assert(header.length == (unsigned int)bytes);
		assert(2 == header.v);

		if(1 == RTCP_P(rtcphd))
		{
			header.length -= *((unsigned char*)data + bytes - 1) * 4;
		}

		switch(RTCP_PT(rtcphd))
		{
		case RTCP_SR:
			rtcp_input_sr(ctx, &header, (const char*)data+4);
			break;

		case RTCP_RR:
			rtcp_input_rr(ctx, &header, (const char*)data+4);
			break;

		case RTCP_SDES:
			rtcp_input_sdes(ctx, &header, (const char*)data+4);
			break;

		case RTCP_BYE:
			rtcp_input_bye(ctx, &header, (const char*)data+4);
			break;

		case RTCP_APP:
			rtcp_input_app(ctx, &header, (const char*)data+4);
			break;

		default:
			assert(0);
		}

		return header.length;
	}
	return -1;
}

void rtcp_input_rtcp(struct rtp_context *ctx, const void* data, int bytes)
{
	int r;
	const unsigned char* p;

	// RFC3550 6.1 RTCP Packet Format
	// 1. The first RTCP packet in the compound packet must always be a report packet to facilitate header validation
	// 2. An SDES packet containing a CNAME item must be included in each compound RTCP packet
	// 3. BYE should be the last packet sent with a given SSRC/CSRC.
	p = (const unsigned char*)data;
	while(bytes > sizeof(unsigned long))
	{
		// compound RTCP packet
		r = rtcp_parse(ctx, p, bytes);
		if(r <= 0)
			break;

		p += r;
		bytes -= r;
	}
}

void rtcp_input_rtp(struct rtp_context *ctx, const void* data, int bytes)
{
	unsigned int v;
	unsigned short seq, maxseq;
	const unsigned int *ptr;
	struct rtp_source *src;

	if(bytes < 3 * sizeof(unsigned int))
		return;

	ptr = (const unsigned int*)data;
	v = ntohl(ptr[0]);

	src = rtp_sender_fetch(ctx, ntohl(ptr[2]));
	src->rtp_timestamp = ntohl(ptr[1]);

	// note: sequence cycle
	seq = RTP_SEQ(v);
	maxseq = src->seq & 0xFFFF;
	if(seq > maxseq)
		src->seq = (src->seq & 0xFFFF0000) | RTP_SEQ(v);
	else if(maxseq + 32 > seq)
		src->seq = ((src->seq & 0xFFFF0000) | RTP_SEQ(v)) + 0x010000;
}

void rtcp_sender_report(struct rtp_context *ctx, void* data, int bytes, time64_t ntp, time_t rtp, unsigned int spc, unsigned int soc)
{
	int i;
	rtcp_sr_t *sr;
	rtcp_rb_t *rb;
	rtcp_header_t header;
	struct rtp_source *src;

	header.v = 2;
	header.p = 0;
	header.pt = RTCP_SR;
	header.rc = rtp_source_list_count(ctx->senders);
	header.length = (sizeof(rtcp_sr_t) + header.rc*sizeof(rtcp_rb_t))/4 - 1;

	sr = (rtcp_sr_t *)data;
	sr->ssrc = htonl(ctx->ssrc);
	sr->ntpts0 = htonl((ntp>>32) & 0xFFFFFFFF);
	sr->ntpts1 = htonl(ntp & 0xFFFFFFFF);
	sr->rtpts = htonl(rtp);
	sr->spc = htonl(spc);
	sr->soc = htonl(soc);

	rb = (rtcp_rb_t *)(sr + 1);
	for(i = 0; i < header.rc; i++, rb++)
	{
		src = rtp_source_list_get(ctx->senders, i);
		rb->ssrc = htonl(src->ssrc);
		rb->fraction = ;
		rb->cumulative = ;
		rb->exthsn = htonl();
		rb->jitter = htonl();
		rb->lsr = htonl();
		rb->dlsr = htonl();
	}
}

void rtcp_receiver_report(struct rtp_context *ctx, void* data, int bytes)
{
	// RFC3550 6.1 RTCP Packet Format
	// An individual RTP participant should send only one compound RTCP packet per report interval
	// in order for the RTCP bandwidth per participant to be estimated correctly (see Section 6.2), 
	// except when the compound RTCP packet is split for partial encryption as described in Section 9.1.
	unsigned int i;
	rtcp_rr_t *rr;
	rtcp_rb_t *rb;
	rtcp_header_t header;
	struct rtp_source *src;

	header.v = 2;
	header.p = 0;
	header.pt = RTCP_RR;
	header.rc = rtp_source_list_count(ctx->senders);
	header.length = (sizeof(rtcp_rr_t) + header.rc*sizeof(rtcp_rb_t)) / 4 - 1;

	rr = (rtcp_rr_t *)data;
	rr->ssrc = htonl(ctx->ssrc);

	rb = (rtcp_rb_t *)(rr + 1);
	for(i = 0; i < header.rc; i++, rb++)
	{
		src = rtp_source_list_get(ctx->senders, i);
		rb->ssrc = htonl(src->ssrc);
		rb->fraction = ;
		rb->cumulative = ;
		rb->exthsn = htonl();
		rb->jitter = htonl();
		rb->lsr = htonl();
		rb->dlsr = htonl();
	}
}

void rtcp_sdes(struct rtp_context *ctx, void* data, int bytes)
{
}
