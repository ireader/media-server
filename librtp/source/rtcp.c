#include "cstringext.h"
#include "rtp-transport.h"
#include <stdio.h>

static void rtcp_input_sr(struct rtp_context *ctx, rtcp_header_t *header, const void* data)
{
	// RFC3550 6.4.1 SR: Sender Report RTCP Packet
	unsigned int i;
	rtcp_sr_t *sr;
	rtcp_rb_t *rb;

	assert(header->length >= sizeof(rtcp_sr_t) + 4);
	sr = (rtcp_sr_t*)data;
	sr->ssrc = ntohl(sr->ssrc);
	sr->ntpts0 = ntohl(sr->ntpts0);
	sr->ntpts1 = ntohl(sr->ntpts1);
	sr->rtpts = ntohl(sr->rtpts);
	sr->spc = ntohl(sr->spc);
	sr->soc = ntohl(sr->soc);

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
	const unsigned char* p;

	assert(header->length >= header->rc * 4 + 4);
	p = (const unsigned char*)data;
	for(i = 0; i < header->rc; i++)
	{
		ssrc = ntohl((unsigned long*)p);
		sdes = (rtcp_sdes_item_t*)(p + 4);
		while(RTCP_SDES_END != sdes->pt)
		{
			switch(sdes->pt)
			{
			case RTCP_SDES_CNAME:
				break;

			case RTCP_SDES_NAME:
				break;

			case RTCP_SDES_EMAIL:
				break;

			case RTCP_SDES_PHONE:
				break;

			case RTCP_SDES_LOC:
				break;

			case RTCP_SDES_TOOL:
				break;

			case RTCP_SDES_NOTE:
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
		// the rst of which is interpreted as an item type of zero to denote the end of the list. 
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
	}

	if(header->length > header->rc * 4 + 4)
	{
		len = *((unsigned char *)data + 4 + header->rc * 4);
		reason = (const char *)data + 4 + header->rc * 4 + 1;
	}
}

static void rtcp_input_app(struct rtp_context *ctx, rtcp_header_t *header, const void* data)
{
	// RFC3550 6.7 APP: Application-Dened RTCP Packet
}

void rtcp_input(struct rtp_context *ctx, const void* data, int bytes)
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

		if(2 != header.v)
		{
		}

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
	}
}

void rtcp_sender_report(struct rtp_context *ctx, void* data, int bytes)
{
	rtcp_header_t header;
	rtcp_sr_t *sr;
	rtcp_rb_t *rb;

	header.v = 2;
	header.p = 0;
	header.pt = RTCP_SR;
	header.rc = 0;
	header.length = 0;

	sr = (rtcp_sr_t *)data;
	sr->ssrc = htonl();
	sr->ntpts0 = htonl();
	sr->ntpts1 = htonl();
	sr->rtpts = htonl();
	sr->spc = htonl();
	sr->soc = htonl();
}

void rtcp_receiver_report(struct rtp_context *ctx, void* data, int bytes)
{
	unsigned int i;
	rtcp_header_t header;
	rtcp_rr_t *rr;
	rtcp_rb_t *rb;

	header.v = 2;
	header.p = 0;
	header.pt = RTCP_RR;

	rr = (rtcp_rr_t *)data;
	rr->ssrc = htonl();

	rb = (rtcp_rb_t *)(rr + 1);
	for(i = 0; i < header.rc; i++, rb++)
	{
		rb->ssrc = htonl();
		rb->fraction = ;
		rb->cumulative = ;
		rb->exthsn = htonl();
		rb->jitter = htonl();
		rb->lsr = htonl();
		rb->dlsr = htonl();
	}
}
