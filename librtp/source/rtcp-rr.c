// RFC3550 6.4.2 RR: Receiver Report RTCP Packet

#include "rtp-internal.h"
#include "rtp-util.h"

void rtcp_rr_unpack(struct rtp_context *ctx, rtcp_header_t *header, const uint8_t* ptr)
{
	uint32_t ssrc, i;
	rtcp_rb_t *rb;
	struct rtp_member *receiver;

	assert(24 == sizeof(rtcp_rb_t) && 4 == sizeof(rtcp_rr_t));
	if (header->length * 4 < 4/*sizeof(rtcp_rr_t)*/ + header->rc * 24/*sizeof(rtcp_rb_t)*/) // RR SSRC + Report Block
	{
		assert(0);
		return;
	}
	ssrc = nbo_r32(ptr);

	receiver = rtp_member_fetch(ctx, ssrc);
	if(!receiver) return; // error

	assert(receiver != ctx->self);
	assert(receiver->rtcp_sr.ssrc == ssrc);
	assert(receiver->rtcp_rb.ssrc == ssrc);
	receiver->rtcp_clock = rtpclock(); // last received clock, for keep-alive

	ptr += 4;
	// report block
	for(i = 0; i < header->rc; i++, ptr+=24/*sizeof(rtcp_rb_t)*/) 
	{
		ssrc = nbo_r32(ptr);
		if(ssrc != ctx->self->ssrc)
			continue; // ignore

		rb = &receiver->rtcp_rb;
		rb->fraction = ptr[4];
		rb->cumulative = (((uint32_t)ptr[5])<<16) | (((uint32_t)ptr[6])<<8)| ptr[7];
		rb->exthsn = nbo_r32(ptr+8);
		rb->jitter = nbo_r32(ptr+12);
		rb->lsr = nbo_r32(ptr+16);
		rb->dlsr = nbo_r32(ptr+20);
	}
}

int rtcp_rr_pack(struct rtp_context *ctx, uint8_t* ptr, int bytes)
{
	// RFC3550 6.1 RTCP Packet Format
	// An individual RTP participant should send only one compound RTCP packet per report interval
	// in order for the RTCP bandwidth per participant to be estimated correctly (see Section 6.2), 
	// except when the compound RTCP packet is split for partial encryption as described in Section 9.1.
	uint32_t i;
	rtcp_header_t header;

	assert(4 == sizeof(rtcp_rr_t));
	assert(24 == sizeof(rtcp_rb_t));
	assert(rtp_member_list_count(ctx->senders) < 32);
	header.v = 2;
	header.p = 0;
	header.pt = RTCP_RR;
	header.rc = MIN(31, rtp_member_list_count(ctx->senders));
	header.length = (4/*sizeof(rtcp_rr_t)*/ + header.rc*24/*sizeof(rtcp_rb_t)*/) / 4;

	if((uint32_t)bytes < 4 + header.length * 4)
		return 4 + header.length * 4;

	nbo_write_rtcp_header(ptr, &header);

	// receiver SSRC
	nbo_w32(ptr+4, ctx->self->ssrc);

	ptr += 8;
	// report block
	for(i = 0; i < header.rc; i++)
	{
		struct rtp_member *sender;

		sender = rtp_member_list_get(ctx->senders, i);
		if(0 == sender->rtp_packets || sender->ssrc == ctx->self->ssrc)
			continue; // don't receive any packet

		ptr += rtcp_report_block(sender, ptr, 24);
	}

	return (header.length+1) * 4;
}
