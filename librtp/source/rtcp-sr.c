// RFC3550 6.4.1 SR: Sender Report RTCP Packet

#include "rtp-internal.h"
#include "rtp-util.h"

time64_t clock2ntp(time64_t clock);

void rtcp_sr_unpack(struct rtp_context *ctx, rtcp_header_t *header, const unsigned char* ptr)
{
	uint32_t ssrc, i;
	rtcp_sr_t *sr;
	rtcp_rb_t *rb;
	struct rtp_member *sender;

	assert(24 == sizeof(rtcp_sr_t));
	assert(24 == sizeof(rtcp_rb_t));
	assert(header->length * 4 >= sizeof(rtcp_sr_t));
	ssrc = be_read_uint32(ptr);

	sender = rtp_sender_fetch(ctx, ssrc);
	if(!sender) return; // error

	assert(sender != ctx->self);
	assert(sender->rtcp_sr.ssrc == ssrc);
	assert(sender->rtcp_rb.ssrc == ssrc);
	sender->rtcp_sr_clock = time64_now();

	// update sender information
	sr = &sender->rtcp_sr;
	sr->ntpmsw = be_read_uint32(ptr + 4);
	sr->ntplsw = be_read_uint32(ptr + 8);
	sr->rtpts = be_read_uint32(ptr + 12);
	sr->spc = be_read_uint32(ptr + 16);
	sr->soc = be_read_uint32(ptr + 20);

	ptr += 24;
	// report block
	for(i = 0; i < header->rc; i++, ptr+=sizeof(rtcp_rb_t)) 
	{
		ssrc = be_read_uint32(ptr);
		if(ssrc != ctx->self->ssrc)
			continue; // ignore

		rb = &sender->rtcp_rb;
		rb->fraction = ptr[4];
		rb->cumulative = (((uint32_t)ptr[5])<<16) | (((uint32_t)ptr[6])<<8)| ptr[7];
		rb->exthsn = be_read_uint32(ptr+8);
		rb->jitter = be_read_uint32(ptr+12);
		rb->lsr = be_read_uint32(ptr+16);
		rb->dlsr = be_read_uint32(ptr+20);
	}
}

size_t rtcp_sr_pack(struct rtp_context *ctx, unsigned char* ptr, size_t bytes)
{
	uint32_t i;
	time64_t ntp;
	rtcp_header_t header;

	assert(24 == sizeof(rtcp_sr_t));
	assert(24 == sizeof(rtcp_rb_t));
	assert(rtp_member_list_count(ctx->senders) < 32);
	header.v = 2;
	header.p = 0;
	header.pt = RTCP_SR;
	header.rc = MIN(31, rtp_member_list_count(ctx->senders));
	header.length = (sizeof(rtcp_sr_t) + header.rc*sizeof(rtcp_rb_t))/4; // see 6.4.1 SR: Sender Report RTCP Packet

	if(bytes < (header.length+1) * 4)
		return (header.length+1) * 4;

	be_write_rtcp_header(ptr, &header);

	ntp = clock2ntp(ctx->self->rtp_clock);
	be_write_uint32(ptr+4, ctx->self->ssrc);
	be_write_uint32(ptr+8, (uint32_t)(ntp >> 32));
	be_write_uint32(ptr+12, (uint32_t)(ntp & 0xFFFFFFFF));
	be_write_uint32(ptr+16, ctx->self->rtp_timestamp);
	be_write_uint32(ptr+20, ctx->self->rtp_packets); // send packets
	be_write_uint32(ptr+24, ctx->self->rtp_octets); // send bytes

	ptr += 28;
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
		assert(sender != ctx->self);

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

		be_write_uint32(ptr, sender->ssrc);
		ptr[4] = (unsigned char)fraction;
		ptr[5] = (unsigned char)((cumulative >> 16) & 0xFF);
		ptr[6] = (unsigned char)((cumulative >> 8) & 0xFF);
		ptr[7] = (unsigned char)(cumulative & 0xFF);
		be_write_uint32(ptr+8, extseq);
		be_write_uint32(ptr+12, (uint32_t)sender->jitter);
		be_write_uint32(ptr+16, lsr);
		be_write_uint32(ptr+20, 0==lsr ? 0 : dlsr);

		sender->rtp_expected = expected; // update source prior data
		sender->rtp_received = sender->rtp_packets;
	}

	return (header.length+1) * 4;
}
