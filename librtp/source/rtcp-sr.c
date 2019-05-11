// RFC3550 6.4.1 SR: Sender Report RTCP Packet

#include "rtp-internal.h"
#include "rtp-util.h"

void rtcp_sr_unpack(struct rtp_context *ctx, rtcp_header_t *header, const uint8_t* ptr)
{
	uint32_t ssrc, i;
	rtcp_sr_t *sr;
	rtcp_rb_t *rb;
	struct rtp_member *sender;

	assert(24 == sizeof(rtcp_sr_t));
	assert(24 == sizeof(rtcp_rb_t));
	if (header->length * 4 < 24/*sizeof(rtcp_sr_t)*/ + header->rc * 24/*sizeof(rtcp_rb_t)*/)
	{
		assert(0);
		return;
	}
	ssrc = nbo_r32(ptr);

	sender = rtp_sender_fetch(ctx, ssrc);
	if(!sender) return; // error

	assert(sender != ctx->self);
	assert(sender->rtcp_sr.ssrc == ssrc);
	assert(sender->rtcp_rb.ssrc == ssrc);
	sender->rtcp_clock = rtpclock();

	// update sender information
	sr = &sender->rtcp_sr;
	sr->ntpmsw = nbo_r32(ptr + 4);
	sr->ntplsw = nbo_r32(ptr + 8);
	sr->rtpts = nbo_r32(ptr + 12);
	sr->spc = nbo_r32(ptr + 16);
	sr->soc = nbo_r32(ptr + 20);

	ptr += 24;
	// report block
	for(i = 0; i < header->rc; i++, ptr+=24/*sizeof(rtcp_rb_t)*/) 
	{
		ssrc = nbo_r32(ptr);
		if(ssrc != ctx->self->ssrc)
			continue; // ignore

		rb = &sender->rtcp_rb;
		rb->fraction = ptr[4];
		rb->cumulative = (((uint32_t)ptr[5])<<16) | (((uint32_t)ptr[6])<<8)| ptr[7];
		rb->exthsn = nbo_r32(ptr+8);
		rb->jitter = nbo_r32(ptr+12);
		rb->lsr = nbo_r32(ptr+16);
		rb->dlsr = nbo_r32(ptr+20);
	}
}

int rtcp_sr_pack(struct rtp_context *ctx, uint8_t* ptr, int bytes)
{
	uint32_t i, timestamp;
	uint64_t ntp;
	rtcp_header_t header;

	assert(24 == sizeof(rtcp_sr_t));
	assert(24 == sizeof(rtcp_rb_t));
	assert(rtp_member_list_count(ctx->senders) < 32);
	header.v = 2;
	header.p = 0;
	header.pt = RTCP_SR;
	header.rc = MIN(31, rtp_member_list_count(ctx->senders));
	header.length = (24/*sizeof(rtcp_sr_t)*/ + header.rc*24/*sizeof(rtcp_rb_t)*/)/4; // see 6.4.1 SR: Sender Report RTCP Packet

	if((uint32_t)bytes < (header.length+1) * 4)
		return (header.length+1) * 4;

	nbo_write_rtcp_header(ptr, &header);

	// RFC3550 6.4.1 SR: Sender Report RTCP Packet (p32)
	// Note that in most cases this timestamp will not be equal to the RTP
	// timestamp in any adjacent data packet. Rather, it must be calculated from the corresponding
	// NTP timestamp using the relationship between the RTP timestamp counter and real time as
	// maintained by periodically checking the wallclock time at a sampling instant.
	ntp = rtpclock();
	if (0 == ctx->self->rtp_packets)
		ctx->self->rtp_clock = ntp;
	timestamp = (uint32_t)((ntp - ctx->self->rtp_clock) * ctx->frequence / 1000) + ctx->self->rtp_timestamp;

	ntp = clock2ntp(ntp);
	nbo_w32(ptr+4, ctx->self->ssrc);
	nbo_w32(ptr+8, (uint32_t)(ntp >> 32));
	nbo_w32(ptr+12, (uint32_t)(ntp & 0xFFFFFFFF));
	nbo_w32(ptr+16, timestamp);
	nbo_w32(ptr+20, ctx->self->rtp_packets); // send packets
	nbo_w32(ptr+24, (uint32_t)ctx->self->rtp_bytes); // send bytes

	ptr += 28;
	// report block
	for(i = 0; i < header.rc; i++, ptr += 24/*sizeof(rtcp_rb_t)*/)
	{
		uint64_t delay;
		int lost_interval;
		int cumulative;
		uint32_t fraction;
		uint32_t expected, extseq;
		uint32_t expected_interval;
		uint32_t received_interval;
		uint32_t lsr, dlsr;
		struct rtp_member *sender;

		sender = rtp_member_list_get(ctx->senders, i);
		if(0 == sender->rtp_packets || sender->ssrc == ctx->self->ssrc)
			continue; // don't receive any packet

		extseq = sender->rtp_seq_cycles + sender->rtp_seq; // 32-bits sequence number
		assert(extseq >= sender->rtp_seq_base);
		expected = extseq - sender->rtp_seq_base + 1;
		expected_interval = expected - sender->rtp_expected0;
		received_interval = sender->rtp_packets - sender->rtp_packets0;
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

		delay = rtpclock() - sender->rtcp_clock; // now - Last SR time
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

		sender->rtp_expected0 = expected; // update source prior data
		sender->rtp_packets0 = sender->rtp_packets;
	}

	return (header.length+1) * 4;
}
