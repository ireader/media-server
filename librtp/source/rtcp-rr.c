// RFC3550 6.4.2 RR: Receiver Report RTCP Packet

#include "rtp-internal.h"
#include "rtp-util.h"

void rtcp_rr_unpack(struct rtp_context *ctx, rtcp_header_t *header, const unsigned char* ptr)
{
	uint32_t ssrc, i;
	rtcp_rb_t *rb;
	struct rtp_member *receiver;

	assert(24 == sizeof(rtcp_rb_t));
	assert(4 == sizeof(rtcp_rr_t));
	assert((header->length+1) * 4 >= sizeof(rtcp_rr_t));
	ssrc = nbo_r32(ptr);

	receiver = rtp_member_fetch(ctx, ssrc);
	if(!receiver) return; // error

	assert(receiver != ctx->self);
	assert(receiver->rtcp_rb.ssrc == ssrc);
	receiver->rtcp_rr_clock = time64_now(); // last received clock, for keep-alive

	ptr += 4;
	// report block
	for(i = 0; i < header->rc; i++, ptr+=sizeof(rtcp_rb_t)) 
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

size_t rtcp_rr_pack(struct rtp_context *ctx, unsigned char* ptr, size_t bytes)
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
	header.length = (sizeof(rtcp_rr_t) + header.rc*sizeof(rtcp_rb_t)) / 4;

	if(bytes < 4 + header.length*4)
		return 4 + header.length*4;

	nbo_write_rtcp_header(ptr, &header);

	// receiver SSRC
	nbo_w32(ptr+4, ctx->self->ssrc);

	ptr += 8;
	// report block
	for(i = 0; i < header.rc; i++, ptr += sizeof(rtcp_rb_t))
	{
		time64_t delay;
		int lost_interval;
		int cumulative;
		uint32_t fraction;
		uint32_t expected, extseq;
		uint32_t expected_interval;
		uint32_t received_interval;
		uint32_t lsr, dlsr;
		struct rtp_member *sender;

		sender = rtp_member_list_get(ctx->senders, i);
		extseq = (sender->seq_cycles << 16) + sender->seq_max; // 32-bits sequence number

		assert(extseq >= sender->seq_base);
		expected = extseq - sender->seq_base + 1;
		expected_interval = expected - sender->rtp_expected;
		received_interval = sender->rtp_packets - sender->rtp_received;
		lost_interval = (int)(expected_interval - received_interval);
		if(lost_interval < 0 || 0 == expected_interval)
			fraction = 0;
		else
			fraction = (lost_interval << 8)/expected_interval;

		cumulative = expected - sender->rtp_packets;
		if(cumulative > 0x007FFFFF)
		{
			cumulative = 0x007FFFFF;
		}
		else if(cumulative < 0)
		{
			// 'Clamp' this loss number to a 24-bit signed value:
			// live555 RTCP.cpp RTCPInstance::enqueueReportBlock line:799
			cumulative = 0;
		}

		delay = time64_now() - sender->rtcp_sr_clock; // now - Last SR time
		lsr = ((sender->rtcp_sr.ntpmsw&0xFFFF)<<16) | ((sender->rtcp_sr.ntplsw>>16) & 0xFFFF);
		// in units of 1/65536 seconds
		// 65536/1000000 == 1024/15625
		dlsr = (uint32_t)(delay/1000.0f * 65536);

		nbo_w32(ptr, sender->ssrc);
		ptr[4] = (unsigned char)fraction;
		ptr[5] = (unsigned char)((cumulative >> 16) & 0xFF);
		ptr[6] = (unsigned char)((cumulative >> 8) & 0xFF);
		ptr[7] = (unsigned char)(cumulative & 0xFF);
		nbo_w32(ptr+8, extseq);
		nbo_w32(ptr+12, (uint32_t)sender->jitter);
		nbo_w32(ptr+16, lsr);
		nbo_w32(ptr+20, 0==lsr ? 0 : dlsr);

		sender->rtp_expected = expected; // update source prior data
		sender->rtp_received = sender->rtp_packets;
	}

	return (header.length+1) * 4;
}
