#include "cstringext.h"
#include "rtp-internal.h"
#include "rtp-packet.h"
#include "time64.h"
#include <stdio.h>
#include <stdlib.h>

time64_t ntp2clock(time64_t ntp);

struct rtp_member* rtp_member_fetch(struct rtp_context *ctx, uint32_t ssrc)
{
	struct rtp_member *p;
	p = rtp_member_list_find(ctx->members, ssrc);
	if(!p)
	{
		// exist in sender list?
		assert(!rtp_member_list_find(ctx->senders, ssrc));

		p = rtp_member_create(ssrc);
		if(p)
		{
			struct rtcp_msg_t msg;

			// update members list
			rtp_member_list_add(ctx->members, p);
			rtp_member_release(p);

			msg.type = RTCP_MSG_MEMBER;
			msg.u.member.ssrc = ssrc;
			ctx->handler.on_rtcp(ctx->cbparam, &msg);
		}
	}
	return p;
}

struct rtp_member* rtp_sender_fetch(struct rtp_context *ctx, uint32_t ssrc)
{
	struct rtp_member *p;
	p = rtp_member_list_find(ctx->senders, ssrc);
	if(!p)
	{
		p = rtp_member_fetch(ctx, ssrc);
		if(p)
		{
			// update senders list
			rtp_member_list_add(ctx->senders, p);
		}
	}
	return p;
}

static int rtcp_parse(struct rtp_context *ctx, const unsigned char* data, size_t bytes)
{
	uint32_t rtcphd;
	rtcp_header_t header;

	assert(bytes >= sizeof(rtcphd));
	rtcphd = be_read_uint32(data);

	header.v = RTCP_V(rtcphd);
	header.p = RTCP_P(rtcphd);
	header.rc = RTCP_RC(rtcphd);
	header.pt = RTCP_PT(rtcphd);
	header.length = RTCP_LEN(rtcphd);
	assert((header.length+1)*4 <= bytes);
	assert(2 == header.v); // 1. RTP version field must equal 2 (p69)
	// 2. The payload type filed of the first RTCP packet in a compound packet must be SR or RR (p69)
	// 3. padding only valid at the last packet

	if(1 == header.p)
	{
		assert((header.length+1)*4 + 4 <= bytes);
		header.length -= *(data + header.length - 1) * 4;
	}

	switch(header.pt)
	{
	case RTCP_SR:
		rtcp_sr_unpack(ctx, &header, data+4);
		break;

	case RTCP_RR:
		rtcp_rr_unpack(ctx, &header, data+4);
		break;

	case RTCP_SDES:
		rtcp_sdes_unpack(ctx, &header, data+4);
		break;

	case RTCP_BYE:
		rtcp_bye_unpack(ctx, &header, data+4);
		break;

	case RTCP_APP:
		rtcp_app_unpack(ctx, &header, data+4);
		break;

	default:
		assert(0);
	}

	return (header.length+1)*4;
}

int rtcp_input_rtcp(struct rtp_context *ctx, const void* data, size_t bytes)
{
	int r;
	const unsigned char* p;

	// RFC3550 6.1 RTCP Packet Format
	// 1. The first RTCP packet in the compound packet must always be a report packet to facilitate header validation
	// 2. An SDES packet containing a CNAME item must be included in each compound RTCP packet
	// 3. BYE should be the last packet sent with a given SSRC/CSRC.
	p = (const unsigned char*)data;
	while(bytes > 4)
	{
		// compound RTCP packet
		r = rtcp_parse(ctx, p, bytes);
		if(r <= 0)
			break;

		// RFC3550 6.3.3 Receiving an RTP or Non-BYE RTCP Packet (p26)
		ctx->avg_rtcp_size = (size_t)(ctx->avg_rtcp_size*1.0/16 + r * 15.0/16);

		p += r;
		bytes -= r;
	}
	return 0;
}

int rtcp_input_rtp(struct rtp_context *ctx, const void* data, size_t bytes, time64_t *time)
{
	time64_t clock;
	rtp_packet_t pkt;
	struct rtp_member *sender;

	if(0 != rtp_packet_deserialize(&pkt, data, bytes))
		return -1; // packet error

	assert(2 == pkt.rtp.v);
	sender = rtp_sender_fetch(ctx, pkt.rtp.ssrc);
	if(!sender)
		return -1;

	clock = time64_now();
	// RFC3550 A.8 Estimating the Interarrival Jitter
	// the jitter estimate is updated:
	if(0 != sender->rtp_clock)
	{
		int D;
		D = (int)((unsigned int)((clock - sender->rtp_clock)*ctx->frequence/1000) - (pkt.rtp.timestamp - sender->rtp_timestamp));
		if(D < 0) D = -D;
		sender->jitter += (D - sender->jitter)/16.0;
	}

	sender->rtp_clock = clock;
	sender->rtp_timestamp = pkt.rtp.timestamp;
	sender->rtp_octets += pkt.payloadlen;
	++sender->rtp_packets;

	// RFC3550 A.1 RTP Data Header Validity Checks
	if(0 == sender->seq_max && 0 == sender->seq_cycles)
	{
		sender->seq_max = (uint16_t)pkt.rtp.seq;
		sender->seq_base = (uint16_t)pkt.rtp.seq;
	}
	else if(pkt.rtp.seq - sender->seq_max < RTP_MISORDER)
	{
		if(pkt.rtp.seq < sender->seq_max)
			sender->seq_cycles += 1;
		sender->seq_max = (uint16_t)pkt.rtp.seq;
	}

	if(0 != sender->rtcp_sr.ntpmsw)
		*time = (int64_t)(sender->rtp_timestamp - sender->rtcp_sr.rtpts)*1000/ctx->frequence + ntp2clock((((time64_t)sender->rtcp_sr.ntpmsw) << 32) | sender->rtcp_sr.ntplsw);
	return 0;
}
