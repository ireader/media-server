#include "rtp-internal.h"
#include "rtp-util.h"
#include <errno.h>

#define TCC01_CLOCK_RESOLUTION 250 // us

static int rtcp_rtpfb_nack_pack(const rtcp_nack_t* nack, int count, uint8_t* ptr, uint32_t bytes);
static int rtcp_rtpfb_tmmbr_pack(const rtcp_tmmbr_t* tmmbr, int count, uint8_t* ptr, uint32_t bytes);
static int rtcp_rtpfb_tmmbn_pack(const rtcp_tmmbr_t* tmmbr, int count, uint8_t* ptr, uint32_t bytes);
static int rtcp_rtpfb_tllei_pack(const rtcp_nack_t* nack, int count, uint8_t* ptr, uint32_t bytes);
static int rtcp_rtpfb_ecn_pack(const rtcp_ecn_t* ecn, uint8_t* ptr, uint32_t bytes);
static int rtcp_rtpfb_ps_pack(uint32_t target, uint8_t cmd, uint8_t len, uint16_t id, const uint8_t* payload, uint8_t* ptr, uint32_t bytes);
static int rtcp_rtpfb_ccfb_pack(uint32_t ssrc, uint16_t begin, const rtcp_ccfb_t* ccfb, int count, uint32_t timestamp, uint8_t* ptr, uint32_t bytes);
static int rtcp_rtpfb_tcc01_pack(uint16_t begin, const rtcp_ccfb_t* ccfb, int count, uint32_t timestamp, uint8_t cc, uint8_t* ptr, uint32_t bytes);

static int rtcp_rtpfb_nack_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_rtpfb_tmmbr_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_rtpfb_tmmbn_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_rtpfb_srreq_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_rtpfb_rams_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_rtpfb_tllei_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_rtpfb_ecn_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_rtpfb_ps_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_rtpfb_dbi_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_rtpfb_ccfb_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_rtpfb_tcc01_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);

// https://datatracker.ietf.org/doc/html/rfc4585#section-6.2.1
/*
	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            PID                |             BLP               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_rtpfb_nack_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	uint32_t i;
	rtcp_nack_t* nack, nack0[32];

	if (bytes / 4 > sizeof(nack0) / sizeof(nack0[0]))
	{
		nack = calloc(bytes / 4, sizeof(*nack));
		if (!nack) return -ENOMEM;
	}
	else
	{
		nack = nack0;
		memset(nack, 0, sizeof(nack[0]) * (bytes / 4));
	}

	for (i = 0; i < bytes / 4; i++)
	{
		nack[i].pid = nbo_r16(ptr);
		nack[i].blp = nbo_r16(ptr + 2);
		ptr += 4;
	}

	msg->u.rtpfb.u.nack.nack = nack;
	msg->u.rtpfb.u.nack.count = i;
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header;
	if (nack && nack != nack0)
		free(nack);
	return 0;
}

static int rtcp_rtpfb_nack_pack(const rtcp_nack_t* nack, int count, uint8_t* ptr, uint32_t bytes)
{
	int i;
	for (i = 0; i < count && bytes >= 4; i++)
	{
		nbo_w16(ptr, nack[i].pid);
		nbo_w16(ptr+2, nack[i].blp);
		
		bytes -= 4;
		ptr += 4;
	}
	return i * 4;
}

// https://www.rfc-editor.org/rfc/rfc5104.html#section-4.2.1
/*
	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                              SSRC                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | MxTBR Exp |  MxTBR Mantissa                 |Measured Overhead|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_rtpfb_tmmbr_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	uint32_t i;
	rtcp_tmmbr_t* tmmbr, tmmbr0[4];

	if (bytes / 8 > sizeof(tmmbr0) / sizeof(tmmbr0[0]))
	{
		tmmbr = calloc(bytes / 8, sizeof(*tmmbr));
		if (!tmmbr) return -ENOMEM;
	}
	else
	{
		tmmbr = tmmbr0;
		memset(tmmbr, 0, sizeof(tmmbr[0]) * (bytes / 8));
	}

	for (i = 0; i < bytes / 8; i++)
	{
		tmmbr[i].ssrc = nbo_r32(ptr);
		tmmbr[i].exp = (ptr[4] >> 2) & 0x3F;
		tmmbr[i].mantissa = ((ptr[4] & 0x03) << 15) | (ptr[5] << 7) | ((ptr[6] >> 1) & 0x7F);
		tmmbr[i].overhead = ((ptr[6] & 0x01) << 8) | ptr[7];
		ptr += 8;
	}

	msg->u.rtpfb.u.tmmbr.tmmbr = tmmbr;
	msg->u.rtpfb.u.tmmbr.count = i;
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header;
	if (tmmbr && tmmbr != tmmbr0)
		free(tmmbr);
	return 0;
}

static int rtcp_rtpfb_tmmbr_pack(const rtcp_tmmbr_t* tmmbr, int count, uint8_t* ptr, uint32_t bytes)
{
	int i;
	for (i = 0; i < count && bytes >= 8; i++)
	{
		nbo_w32(ptr, tmmbr[i].ssrc);
		nbo_w32(ptr + 4, ((tmmbr[i].exp & 0x3F) << 26) | ((tmmbr[i].mantissa & 0x1FFFF) << 9) | (tmmbr[i].overhead & 0x1FF));

		bytes -= 8;
		ptr += 8;
	}
	return i * 8;
}

// https://www.rfc-editor.org/rfc/rfc5104.html#section-4.2.2
static int rtcp_rtpfb_tmmbn_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	uint32_t i;
	rtcp_tmmbr_t* tmmbr, tmmbr0[4];

	if (bytes / 8 > sizeof(tmmbr0) / sizeof(tmmbr0[0]))
	{
		tmmbr = calloc(bytes / 8, sizeof(*tmmbr));
		if (!tmmbr) return -ENOMEM;
	}
	else
	{
		tmmbr = tmmbr0;
		memset(tmmbr, 0, sizeof(tmmbr[0]) * (bytes / 8));
	}

	for (i = 0; i < bytes / 8; i++)
	{
		tmmbr[i].ssrc = nbo_r32(ptr);
		tmmbr[i].exp = (ptr[4] >> 2) & 0x3F;
		tmmbr[i].mantissa = ((ptr[4] & 0x03) << 15) | (ptr[5] << 7) | ((ptr[6] >> 1) & 0x7F);
		tmmbr[i].overhead = ((ptr[6] & 0x01) << 8) | ptr[7];
		ptr += 8;
	}

	msg->u.rtpfb.u.tmmbr.tmmbr = tmmbr;
	msg->u.rtpfb.u.tmmbr.count = i;
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header;
	if (tmmbr && tmmbr != tmmbr0)
		free(tmmbr);
	return 0;
}

static int rtcp_rtpfb_tmmbn_pack(const rtcp_tmmbr_t* tmmbr, int count, uint8_t* ptr, uint32_t bytes)
{
	return rtcp_rtpfb_tmmbr_pack(tmmbr, count, ptr, bytes);
}

// https://www.rfc-editor.org/rfc/rfc6051.html#section-3.2
static int rtcp_rtpfb_srreq_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	// The SSRC of the packet sender indicates the member that is unable to synchronise media streams, 
	// while the SSRC of the media source indicates the sender of the media it is unable to synchronise.
	// The length MUST equal 2.
	assert(bytes == 0);
	(void)ctx, (void)header, (void)msg, (void)ptr;
	return 0;
}

// https://www.rfc-editor.org/rfc/rfc6285.html#section-7
/*
	  0                   1                   2                   3
	  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 |     Type      |   Reserved    |            Length             |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 :                             Value                             :
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

				   Figure 5: Structure of a TLV Element

	  0                   1                   2                   3
	  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 |    SFMT=1     |                    Reserved                   |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 :                  Requested Media Sender SSRC(s)               :
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 :      Optional TLV-encoded Fields (and Padding, if needed)     :
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

		  Figure 7: FCI Field Syntax for the RAMS Request Message

	  0                   1                   2                   3
	  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 |    SFMT=2     |      MSN      |          Response             |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 :      Optional TLV-encoded Fields (and Padding, if needed)     :
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

		Figure 8: FCI Field Syntax for the RAMS Information Message
*/
static int rtcp_rtpfb_rams_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	uint8_t sfmt;
	if (bytes < 4)
		return -1;
	sfmt = ptr[0];
	(void)ctx, (void)header, (void)msg;
	return 0;
}

// https://www.rfc-editor.org/rfc/rfc6642.html#section-5.1
/*
	   0                   1                   2                   3
	   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	  |            PID                |             BLP               |
	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_rtpfb_tllei_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	uint32_t i;
	rtcp_nack_t* nack, nack0[32];

	if (bytes / 4 > sizeof(nack0) / sizeof(nack0[0]))
	{
		nack = calloc(bytes / 4, sizeof(*nack));
		if (!nack) return -ENOMEM;
	}
	else
	{
		nack = nack0;
		memset(nack, 0, sizeof(nack[0]) * (bytes / 4));
	}

	for (i = 0; i < bytes / 4; i++)
	{
		nack[i].pid = nbo_r16(ptr);
		nack[i].blp = nbo_r16(ptr + 2);
		ptr += 4;
	}

	msg->u.rtpfb.u.nack.nack = nack;
	msg->u.rtpfb.u.nack.count = i;
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header;
	if (nack && nack != nack0)
		free(nack);
	return 0;
}

static int rtcp_rtpfb_tllei_pack(const rtcp_nack_t* nack, int count, uint8_t* ptr, uint32_t bytes)
{
	return rtcp_rtpfb_nack_pack(nack, count, ptr, bytes);
}

// https://www.rfc-editor.org/rfc/rfc6679.html#section-5.1
/*
	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Extended Highest Sequence Number                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | ECT (0) Counter                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | ECT (1) Counter                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | ECN-CE Counter                | not-ECT Counter               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Lost Packets Counter          | Duplication Counter           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_rtpfb_ecn_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	rtcp_ecn_t ecn;
	if (bytes < 20)
		return -1;
	ecn.ext_highest_seq = nbo_r32(ptr);
	ecn.ect[0] = nbo_r32(ptr + 4);
	ecn.ect[1] = nbo_r32(ptr + 8);
	ecn.ect_ce_counter = nbo_r16(ptr + 12);
	ecn.not_ect_counter = nbo_r16(ptr + 14);
	ecn.lost_packets_counter = nbo_r16(ptr + 16);
	ecn.duplication_counter = nbo_r16(ptr + 18);

	memcpy(&msg->u.rtpfb.u.ecn, &ecn, sizeof(ecn));
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header;
	return 0;
}

static int rtcp_rtpfb_ecn_pack(const rtcp_ecn_t* ecn, uint8_t* ptr, uint32_t bytes)
{
	if (bytes < 20)
		return -1;

	nbo_w32(ptr, ecn->ext_highest_seq);
	nbo_w32(ptr + 4, ecn->ect[0]);
	nbo_w32(ptr + 8, ecn->ect[1]);
	nbo_w16(ptr + 12, ecn->ect_ce_counter);
	nbo_w16(ptr + 14, ecn->not_ect_counter);
	nbo_w16(ptr + 16, ecn->lost_packets_counter);
	nbo_w16(ptr + 18, ecn->duplication_counter);
	return 20;
}

// https://www.rfc-editor.org/rfc/rfc7728.html#section-7
/*
	  0                   1                   2                   3
	  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 |                           Target SSRC                         |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 | Type  |  Res  | Parameter Len |           PauseID             |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 :                         Type Specific                         :
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_rtpfb_ps_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	uint16_t id;
	uint32_t target;
	uint8_t cmd;
	uint32_t len;

	while(bytes >= 8)
	{
		target = nbo_r32(ptr); // target ssrc
		cmd = ptr[4] >> 4;
		len = ptr[5];
		id = nbo_r16(ptr + 6);

		if (len * 4 + 8 > bytes)
		{
			assert(0);
			return -1;
		}

		msg->u.rtpfb.u.ps.target = target;
		msg->u.rtpfb.u.ps.cmd = cmd;
		msg->u.rtpfb.u.ps.len = len;
		msg->u.rtpfb.u.ps.id = id;
		msg->u.rtpfb.u.ps.payload = (uint8_t*)ptr + 8;
		ctx->handler.on_rtcp(ctx->cbparam, msg);

		ptr += 8 + len * 4;
		bytes -= 8 + len * 4;
	}

	(void)ctx, (void)header;
	return 0;
}

static int rtcp_rtpfb_ps_pack(uint32_t target, uint8_t cmd, uint8_t len, uint16_t id, const uint8_t* payload, uint8_t* ptr, uint32_t bytes)
{
	if (bytes < 8 + (uint32_t)len * 4)
		return -1;

	nbo_w32(ptr, target);
	nbo_w32(ptr + 4, ((cmd & 0x0F) << 28) | ((len & 0xFF) << 16) | id);
	if (len > 0)
		memcpy(ptr + 8, payload, len * 4);
	return 8 + len * 4;
}

// https://portal.3gpp.org/desktopmodules/Specifications/SpecificationDetails.aspx?specificationId=1404
/*
 0                1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            delay              |s|q|     zero padding          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_rtpfb_dbi_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	if (bytes < 4)
		return -1;

	msg->u.rtpfb.u.dbi.delay = nbo_r16(ptr);
	msg->u.rtpfb.u.dbi.s = (ptr[2] >> 7) & 0x01;
	msg->u.rtpfb.u.dbi.q = (ptr[2] >> 6) & 0x01;
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header, (void)ptr;
	return 0;
}

// https://www.rfc-editor.org/rfc/rfc8888.html#section-3.1
/*
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |V=2|P| FMT=11  |   PT = 205    |          length               |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                 SSRC of RTCP packet sender                    |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                   SSRC of 1st RTP Stream                      |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |          begin_seq            |          num_reports          |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |R|ECN|  Arrival time offset    | ...                           .
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  .                                                               .
  .                                                               .
  .                                                               .
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                   SSRC of nth RTP Stream                      |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |          begin_seq            |          num_reports          |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |R|ECN|  Arrival time offset    | ...                           |
  .                                                               .
  .                                                               .
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                 Report Timestamp (32 bits)                    |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_rtpfb_ccfb_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	uint32_t i, num, ssrc;
	uint32_t timestamp;
	uint16_t begin;
	rtcp_ccfb_t* ccfb, ccfb0[32];

	timestamp = bytes >= 4 ? nbo_r32(ptr + bytes - 4) : 0; // get last report timestamp

	while(bytes >= 8)
	{
		ssrc = nbo_r32(ptr); // target ssrc
		begin = nbo_r16(ptr + 4);
		num = nbo_r16(ptr + 6);

		ptr += 8;
		bytes -= 8;
		if ( (num * 2 + 3) / 4 * 4 > bytes) // 4-bytes padding
		{
			assert(0);
			return -1;
		}

		if (num > sizeof(ccfb0) / sizeof(ccfb0[0]))
		{
			ccfb = calloc(num, sizeof(*ccfb));
			if (!ccfb) return -ENOMEM;
		}
		else
		{
			ccfb = ccfb0;
			memset(ccfb, 0, sizeof(ccfb[0]) * num);
		}

		for (i = 0; i < num; i++)
		{
			ccfb[i].seq = begin + i;
			ccfb[i].received = (ptr[i * 2] >> 7) & 0x01;
			ccfb[i].ecn = (ptr[i * 2] >> 5) & 0x03;
			ccfb[i].ato = ((ptr[i * 2] & 0x1F) << 8) | ptr[i * 2 + 1];
		}

		msg->u.rtpfb.u.tcc01.timestamp = 0; // fixme
		msg->u.rtpfb.u.tcc01.ssrc = ssrc;
		msg->u.rtpfb.u.tcc01.begin = begin;
		msg->u.rtpfb.u.tcc01.ccfb = ccfb;
		msg->u.rtpfb.u.tcc01.count = i;
		ctx->handler.on_rtcp(ctx->cbparam, msg);

		num = (num + 1) / 2 * 2; // 4-bytes padding
		assert(bytes >= num * 2); // check again
		ptr += num * 2;
		bytes -= num * 2;

		if (ccfb && ccfb != ccfb0)
			free(ccfb);
	}

	if (bytes >= 4)
	{
		timestamp = nbo_r32(ptr);
		ptr += 4;
		bytes -= 4;
	}

	(void)ctx, (void)header;
	assert(0 == bytes);
	return 0;
}

static int rtcp_rtpfb_ccfb_pack(uint32_t ssrc, uint16_t begin, const rtcp_ccfb_t* ccfb, int count, uint32_t timestamp, uint8_t* ptr, uint32_t bytes)
{
	int i;
	if (bytes < 8 + (uint32_t)count * 2 + 4 || count > 0xFFFF)
		return -1;

	nbo_w32(ptr, ssrc);
	nbo_w16(ptr + 4, begin);
	nbo_w16(ptr + 6, (uint16_t)count);
	ptr += 8;

	for (i = 0; i < count; i++)
	{
		nbo_w16(ptr, (ccfb[i].received ? 0x8000 : 0) | ((ccfb[i].ecn & 0x03) << 13) | (ccfb->ato & 0x1FFF));
		bytes -= 2;
		ptr += 2;
	}

	nbo_w32(ptr, timestamp);
	return 8 + count * 2 + 4;
}

// https://datatracker.ietf.org/doc/html/draft-holmer-rmcat-transport-wide-cc-extensions-00#section-3.1
/*
	 0                   1                   2                   3
	  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 | fb seq num                  |r|       base sequence number    |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 |       base receive time       |  sequence number ack vector   |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 | recv delta        | recv delta        | recv delta        |...|
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 .                                                               .
	 .                                                               .
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 | recovery base sequence number |  recovery vector              |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

// https://datatracker.ietf.org/doc/html/draft-holmer-rmcat-transport-wide-cc-extensions#section-3.1
/*
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |V=2|P|  FMT=15 |    PT=205     |           length              |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                     SSRC of packet sender                     |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                      SSRC of media source                     |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |      base sequence number     |      packet status count      |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                 reference time                | fb pkt. count |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |          packet chunk         |         packet chunk          |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       .                                                               .
       .                                                               .
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |         packet chunk          |  recv delta   |  recv delta   |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       .                                                               .
       .                                                               .
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |           recv delta          |  recv delta   | zero padding  |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_rtpfb_tcc01_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	int r;
	uint16_t i, j;
	uint32_t timestamp;
	uint16_t seq, num, chunk;
	uint8_t cc;
	rtcp_ccfb_t* ccfb, ccfb0[32];

	if (bytes < 8)
		return -1;
	seq = nbo_r16(ptr);
	num = nbo_r16(ptr + 2);
	timestamp = (ptr[4] << 16) | (ptr[5] << 8) | ptr[6];
	cc = ptr[7];
	msg->u.rtpfb.u.tcc01.cc = cc;
	msg->u.rtpfb.u.tcc01.begin = seq;
	msg->u.rtpfb.u.tcc01.timestamp = timestamp | ((timestamp & 0x00800000) ? 0xFF000000 : 0); // signed-24 -> signed-32

	if (num > sizeof(ccfb0) / sizeof(ccfb0[0]))
	{
		ccfb = calloc(num, sizeof(*ccfb));
		if (!ccfb) return -ENOMEM;
	}
	else
	{
		ccfb = ccfb0;
		memset(ccfb, 0, sizeof(ccfb[0]) * num);
	}

	r = 0;
	ptr += 8;
	bytes -= 8;
	for (i = 0; bytes >= 2 && i < num; bytes -= 2, ptr += 2)
	{
		chunk = nbo_r16(ptr); // packet chunk

		if (0 == (0x8000 & chunk))
		{
			// Run Length Chunk
			for (j = 0; j < (uint16_t)(chunk & 0x1FFF) && i < num; j++, i++)
			{
				ccfb[i].seq = seq++;
				ccfb[i].ecn = (chunk >> 13) & 0x03;
				ccfb[i].received = ccfb[i].ecn ? 1 : 0;
				ccfb[i].ato = 0;
			}
		}
		else
		{
			// Status Vector Chunk
			if (0x4000 & chunk)
			{
				// two bits
				for (j = 0; j < 7 && i < num; j++, i++)
				{
					ccfb[i].seq = seq++;
					ccfb[i].ecn = (chunk >> (2 * (6 - j))) & 0x03;
					ccfb[i].received = ccfb[i].ecn ? 1 : 0;
					ccfb[i].ato = 0;
				}
			}
			else
			{
				// one bits
				for (j = 0; j < 14 && i < num; j++, i++)
				{
					ccfb[i].seq = seq++;
					ccfb[i].ecn = (chunk & (1 << (13 - j))) ? 0x01 : 0x00; // small delta
					ccfb[i].received = ccfb[i].ecn ? 1 : 0;
					ccfb[i].ato = 0;
				}
			}
		}
	}

	for (i = 0; i < num && bytes > 0; i++)
	{
		if (!ccfb[i].received)
			continue;

		assert(ccfb[i].ecn == 0x01 || ccfb[i].ecn == 0x02);
		if (ccfb[i].ecn == 0x01)
		{
			ccfb[i].ato = ptr[0] >> 2; // 250us
			bytes -= 1;
			ptr += 1;
		}
		else
		{
			if (bytes < 2)
			{
				assert(0);
				r = -1;
				break;
			}
			ccfb[i].ato = ((int16_t)nbo_r16(ptr)) >> 2; // 250us -> 1/1024(s)
			bytes -= 2;
			ptr += 2;
		}
	}

	if (0 == r)
	{
		msg->u.rtpfb.u.tcc01.ccfb = ccfb;
		msg->u.rtpfb.u.tcc01.count = num;
		ctx->handler.on_rtcp(ctx->cbparam, msg);
	}
	(void)ctx, (void)header;
	if (ccfb && ccfb != ccfb0)
		free(ccfb);
	return r;
}

static int rtcp_rtpfb_tcc01_pack(uint16_t begin, const rtcp_ccfb_t* ccfb, int count, uint32_t timestamp, uint8_t cc, uint8_t* ptr, uint32_t bytes)
{
	int i, k, n;
	int16_t ato;
	uint16_t chunk, two;
	uint32_t seq, received;
	const uint8_t* p;
	if (bytes < 8 || count < 1)
		return -1;

	nbo_w16(ptr, begin);
	nbo_w16(ptr + 2, (uint16_t)count); // placeholder
	nbo_w32(ptr + 4, ((timestamp & 0xFFFFFF) << 8) | cc);
	ptr += 8;
	p = ptr; // chunk pointer
	n = 8;
	
	for (i = 0; i < count && bytes >= 2; i += k)
	{
		ato = ccfb[i].ato << 2; // 1/1024(s) -> 250us
		two = (ccfb[i].received && (uint16_t)ato > 0xFF) ? 2 : 1;
		received = ccfb[i].received;
		
		// try Run Length Chunk
		for (k = 1; i + k < count && ccfb[i + k].received == ccfb[i].received; k++)
		{
			ato = ccfb[i + k].ato << 2; // 1/1024(s) -> 250us
			two = (ccfb[i + k].received && (uint16_t)ato > 0xFF) ? 2 : two;
			received |= ccfb[i + k].received;
		}

		if (k > 14 / two)
		{
			// Run Length Chunk
			chunk = 0x0000 | (received ? (2 == two ? 0x4000 : 0x2000) : 0x0000) | (uint16_t)k;
		}
		else
		{
			two = 1; // re-detect
			for (k = 0; k < 14 / two && i + k < count; k++)
			{
				ato = ccfb[i + k].ato << 2; // 1/1024(s) -> 250us
				two = (ccfb[i + k].received && (uint16_t)ato > 0xFF) ? 2 : two;
			}

			ato = (2 == two || k <= 7) ? 1 : 0; // small space/padding
			chunk = 0x8000 | (ato ? 0x4000 : 0x0000);
			for (k = 0; k < 14 / two && i + k < count; k++)
			{
				if (ccfb[i + k].received)
				{
					chunk |= ((uint16_t)(ccfb[i + k].ato << 2) > 0xFF ? 2 : 1) << (ato ? (12 - k * 2) : (13 - k));
				}
			}
		}

		nbo_w16(ptr, chunk);
		bytes -= 2;
		ptr += 2;
		n += 2;
	}

	// parse chunk and write delta
	for (i = 0; i < count && bytes >= 1; p += 2)
	{
		chunk = nbo_r16(p); // packet chunk

		if (0 == (0x8000 & chunk))
		{
			// Run Length Chunk
			seq = ccfb[i].seq;
			for (k = 0; k < (uint16_t)(chunk & 0x1FFF) && i < count; k++)
			{
				assert(seq + k == ccfb[i].seq);
				if (seq + k != ccfb[i].seq)
					continue;

				ato = ccfb[i].ato << 2; // 1/1024(s) -> 250us
				if (chunk & 0x4000)
				{
					if (bytes < 2)
						return -1;
					nbo_w16(ptr, ato);
					bytes -= 2;
					ptr += 2;
					n += 2;
				}				
				else if (chunk & 0x2000)
				{
					if (bytes < 1)
						return -1;
					ptr[0] = (uint8_t)ato;
					bytes -= 1;
					ptr += 1;
					n += 1;
				}

				i++;
			}
		}
		else
		{
			// Status Vector Chunk
			if (0x4000 & chunk)
			{
				// two bits
				for (k = 0; k < 7 && i < count && bytes >= 2; k++, i++)
				{
					if (!ccfb[i].received)
						continue;

					ato = ccfb[i].ato << 2; // 1/1024(s) -> 250us
					two = (chunk >> (2 * (6 - k))) & 0x03;
					if (two == 1)
					{
						ptr[0] = (uint8_t)ato;
						bytes -= 1;
						ptr += 1;
						n += 1;
					}
					else
					{
						nbo_w16(ptr, ato);
						bytes -= 2;
						ptr += 2;
						n += 2;
					}
				}
			}
			else
			{
				// one bits
				for (k = 0; k < 14 && i < count && bytes >= 1; k++, i++)
				{
					if (!ccfb[i].received)
						continue;

					ato = ccfb[i].ato << 2; // 1/1024(s) -> 250us
					ptr[0] = (uint8_t)ato;
					bytes -= 1;
					ptr += 1;
					n += 1;
				}
			}
		}
	}

	// padding
	for(k = 0; k < 4 && (n % 4 != 0) && bytes > 0; k++)
	{
		ptr[0] = ((n + 1) % 4 == 0) ? (uint8_t)k + 1 : 0;
		bytes -= 1;
		ptr += 1;
		n += 1;
	}
	return n;
}

// https://datatracker.ietf.org/doc/html/rfc4585#section-6.2
/*
*  Common Packet Format for Feedback Messages

	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|   FMT   |       PT      |          length               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  SSRC of packet sender                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  SSRC of media source                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   :            Feedback Control Information (FCI)                 :
   :                                                               :
*/
void rtcp_rtpfb_unpack(struct rtp_context* ctx, const rtcp_header_t* header, const uint8_t* ptr, size_t bytes)
{
	int r;
	struct rtcp_msg_t msg;
	struct rtp_member* sender;

	if (bytes < 8 /*sizeof(rtcp_fci_t)*/)
	{
		assert(0);
		return;
	}

	msg.type = RTCP_RTPFB | (header->rc << 8);
	msg.ssrc = nbo_r32(ptr);
	msg.u.rtpfb.media = nbo_r32(ptr + 4);

	sender = rtp_sender_fetch(ctx, msg.ssrc);
	if (!sender) return; // error
	//assert(sender != ctx->self);

	switch (header->rc)
	{
	case RTCP_RTPFB_NACK:
		r = rtcp_rtpfb_nack_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_RTPFB_TMMBR:
		r = rtcp_rtpfb_tmmbr_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_RTPFB_TMMBN:
		r = rtcp_rtpfb_tmmbn_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_RTPFB_SRREQ:
		r = rtcp_rtpfb_srreq_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_RTPFB_RAMS:
		r = rtcp_rtpfb_rams_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_RTPFB_TLLEI:
		r = rtcp_rtpfb_tllei_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_RTPFB_ECN:
		r = rtcp_rtpfb_ecn_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_RTPFB_PS:
		r = rtcp_rtpfb_ps_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_RTPFB_DBI:
		r = rtcp_rtpfb_dbi_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_RTPFB_CCFB:
		r = rtcp_rtpfb_ccfb_unpack(ctx, header, &msg, ptr + 4 /*1st ssrc*/, bytes - 4);
		break;

	case RTCP_RTPFB_TCC01:
		r = rtcp_rtpfb_tcc01_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	default:
		assert(0);
		r = 0;
		break;
	}

	return;
}

int rtcp_rtpfb_pack(struct rtp_context* ctx, uint8_t* data, int bytes, enum rtcp_rtpfb_type_t id, const rtcp_rtpfb_t* rtpfb)
{
	int r;
	rtcp_header_t header;

	(void)ctx;
	if (bytes < 4 + 4 + 4)
		return 4 + 4 + 4;

	switch (id)
	{
	case RTCP_RTPFB_NACK:
		r = rtcp_rtpfb_nack_pack(rtpfb->u.nack.nack, rtpfb->u.nack.count, data + 12, bytes - 12);
		break;

	case RTCP_RTPFB_TMMBR:
		r = rtcp_rtpfb_tmmbr_pack(rtpfb->u.tmmbr.tmmbr, rtpfb->u.tmmbr.count, data + 12, bytes - 12);
		break;

	case RTCP_RTPFB_TMMBN:
		r = rtcp_rtpfb_tmmbn_pack(rtpfb->u.tmmbr.tmmbr, rtpfb->u.tmmbr.count, data + 12, bytes - 12);
		break;

	case RTCP_RTPFB_TLLEI:
		r = rtcp_rtpfb_tllei_pack(rtpfb->u.nack.nack, rtpfb->u.nack.count, data + 12, bytes - 12);
		break;

	case RTCP_RTPFB_ECN:
		r = rtcp_rtpfb_ecn_pack(&rtpfb->u.ecn, data + 12, bytes - 12);
		break;

	case RTCP_RTPFB_PS:
		r = rtcp_rtpfb_ps_pack(rtpfb->u.ps.target, (uint8_t)rtpfb->u.ps.cmd, (uint8_t)rtpfb->u.ps.len, (uint16_t)rtpfb->u.ps.id, (const uint8_t*)rtpfb->u.ps.payload, data + 12, bytes - 12);
		break;

	case RTCP_RTPFB_CCFB:
		r = rtcp_rtpfb_ccfb_pack(rtpfb->u.tcc01.ssrc, (uint16_t)rtpfb->u.tcc01.begin, rtpfb->u.tcc01.ccfb, rtpfb->u.tcc01.count, rtpfb->u.tcc01.timestamp, data + 12, bytes - 12);
		break;

	case RTCP_RTPFB_TCC01:
		r = rtcp_rtpfb_tcc01_pack((uint16_t)rtpfb->u.tcc01.begin, rtpfb->u.tcc01.ccfb, rtpfb->u.tcc01.count, rtpfb->u.tcc01.timestamp, (uint8_t)rtpfb->u.tcc01.cc, data + 12, bytes - 12);
		break;

	case RTCP_RTPFB_SRREQ:
	case RTCP_RTPFB_RAMS:
	case RTCP_RTPFB_DBI:
	default:
		assert(0);
		return -1;
	}

	header.v = 2;
	header.p = 0;
	header.pt = RTCP_RTPFB;
	header.rc = id;
	header.length = (r + 8 + 3) / 4;
	nbo_write_rtcp_header(data, &header);

	nbo_w32(data + 4, ctx->self->ssrc);
	//nbo_w32(data + 4, rtpfb->sender);
	nbo_w32(data + 8, rtpfb->media);

	//assert(8 == (header.length + 1) * 4);
	return header.length * 4 + 4;
}

#if defined(_DEBUG) || defined(DEBUG)
static void rtcp_on_rtpfb_test(void* param, const struct rtcp_msg_t* msg)
{
	int r;
	static uint8_t buffer[1400];
	switch (msg->type & 0xFF)
	{
	case RTCP_RTPFB:
		switch ((msg->type >> 8) & 0xFF)
		{
		case RTCP_RTPFB_NACK:
			assert(13 == msg->u.rtpfb.u.nack.count);
			assert(msg->u.rtpfb.u.nack.nack[0].pid == 631 && msg->u.rtpfb.u.nack.nack[0].blp == 0x8028);
			assert(msg->u.rtpfb.u.nack.nack[1].pid == 648 && msg->u.rtpfb.u.nack.nack[1].blp == 0x0021);
			assert(msg->u.rtpfb.u.nack.nack[2].pid == 666 && msg->u.rtpfb.u.nack.nack[2].blp == 0x0008);
			assert(msg->u.rtpfb.u.nack.nack[3].pid == 690 && msg->u.rtpfb.u.nack.nack[3].blp == 0x2000);
			assert(msg->u.rtpfb.u.nack.nack[4].pid == 734 && msg->u.rtpfb.u.nack.nack[4].blp == 0x0025);
			assert(msg->u.rtpfb.u.nack.nack[5].pid == 757 && msg->u.rtpfb.u.nack.nack[5].blp == 0x1100);
			assert(msg->u.rtpfb.u.nack.nack[6].pid == 777 && msg->u.rtpfb.u.nack.nack[6].blp == 0x0000);
			assert(msg->u.rtpfb.u.nack.nack[7].pid == 826 && msg->u.rtpfb.u.nack.nack[7].blp == 0x0002);
			assert(msg->u.rtpfb.u.nack.nack[8].pid == 865 && msg->u.rtpfb.u.nack.nack[8].blp == 0x0000);
			assert(msg->u.rtpfb.u.nack.nack[9].pid == 882 && msg->u.rtpfb.u.nack.nack[9].blp == 0x0300);
			assert(msg->u.rtpfb.u.nack.nack[10].pid == 907 && msg->u.rtpfb.u.nack.nack[10].blp == 0x0400);
			assert(msg->u.rtpfb.u.nack.nack[11].pid == 931 && msg->u.rtpfb.u.nack.nack[11].blp == 0x0000);
			assert(msg->u.rtpfb.u.nack.nack[12].pid == 963 && msg->u.rtpfb.u.nack.nack[12].blp == 0x006d);
			r = rtcp_rtpfb_nack_pack(msg->u.rtpfb.u.nack.nack, msg->u.rtpfb.u.nack.count, buffer, sizeof(buffer));
			assert(r > 0 && 0 == memcmp(buffer, param, r));
			break;

		case RTCP_RTPFB_TMMBR:
			assert(1 == msg->u.rtpfb.u.tmmbr.count);
			assert(0x23456789 == msg->u.rtpfb.u.tmmbr.tmmbr[0].ssrc && 2 == msg->u.rtpfb.u.tmmbr.tmmbr[0].exp && 78000 == msg->u.rtpfb.u.tmmbr.tmmbr[0].mantissa && 312000 == (msg->u.rtpfb.u.tmmbr.tmmbr[0].mantissa << msg->u.rtpfb.u.tmmbr.tmmbr[0].exp) && 0x1fe == msg->u.rtpfb.u.tmmbr.tmmbr[0].overhead);
			r = rtcp_rtpfb_tmmbr_pack(msg->u.rtpfb.u.tmmbr.tmmbr, msg->u.rtpfb.u.tmmbr.count, buffer, sizeof(buffer));
			assert(8 == r && 0 == memcmp(buffer, param, r));
			break;

		case RTCP_RTPFB_TMMBN:
			assert(1 == msg->u.rtpfb.u.tmmbr.count);
			assert(0x23456789 == msg->u.rtpfb.u.tmmbr.tmmbr[0].ssrc && 2 == msg->u.rtpfb.u.tmmbr.tmmbr[0].exp && 78000 == msg->u.rtpfb.u.tmmbr.tmmbr[0].mantissa && 312000 == (msg->u.rtpfb.u.tmmbr.tmmbr[0].mantissa << msg->u.rtpfb.u.tmmbr.tmmbr[0].exp) && 0x1fe == msg->u.rtpfb.u.tmmbr.tmmbr[0].overhead);
			r = rtcp_rtpfb_tmmbr_pack(msg->u.rtpfb.u.tmmbr.tmmbr, msg->u.rtpfb.u.tmmbr.count, buffer, sizeof(buffer));
			assert(8 == r && 0 == memcmp(buffer, param, r));
			break;

		case RTCP_RTPFB_TCC01:
			assert(1767 == msg->u.rtpfb.u.tcc01.begin && 4114000 == msg->u.rtpfb.u.tcc01.timestamp && 101 == msg->u.rtpfb.u.tcc01.count && 42 == msg->u.rtpfb.u.tcc01.cc);
			assert(msg->u.rtpfb.u.tcc01.ccfb[0].seq == 1767 && msg->u.rtpfb.u.tcc01.ccfb[0].received == 0);
			assert(msg->u.rtpfb.u.tcc01.ccfb[1].seq == 1768 && msg->u.rtpfb.u.tcc01.ccfb[1].received == 1 && msg->u.rtpfb.u.tcc01.ccfb[1].ato == 21);
			assert(msg->u.rtpfb.u.tcc01.ccfb[2].seq == 1769 && msg->u.rtpfb.u.tcc01.ccfb[2].received == 1 && msg->u.rtpfb.u.tcc01.ccfb[2].ato == 0);
			assert(msg->u.rtpfb.u.tcc01.ccfb[3].seq == 1770 && msg->u.rtpfb.u.tcc01.ccfb[3].received == 0);
			assert(msg->u.rtpfb.u.tcc01.ccfb[4].seq == 1771 && msg->u.rtpfb.u.tcc01.ccfb[4].received == 1 && msg->u.rtpfb.u.tcc01.ccfb[4].ato == 0);
			assert(msg->u.rtpfb.u.tcc01.ccfb[5].seq == 1772 && msg->u.rtpfb.u.tcc01.ccfb[5].received == 0);
			assert(msg->u.rtpfb.u.tcc01.ccfb[6].seq == 1773 && msg->u.rtpfb.u.tcc01.ccfb[6].received == 1 && msg->u.rtpfb.u.tcc01.ccfb[6].ato == 26);
			r = rtcp_rtpfb_tcc01_pack((uint16_t)msg->u.rtpfb.u.tcc01.begin, msg->u.rtpfb.u.tcc01.ccfb, msg->u.rtpfb.u.tcc01.count, msg->u.rtpfb.u.tcc01.timestamp, (uint8_t)msg->u.rtpfb.u.tcc01.cc, buffer, sizeof(buffer));
			assert(104 == r && 0 == memcmp(buffer, param, r));
			break;

		case 30: // test only
			assert(5 == msg->u.rtpfb.u.tcc01.begin && 5 == msg->u.rtpfb.u.tcc01.timestamp && 7 == msg->u.rtpfb.u.tcc01.count && 0 == msg->u.rtpfb.u.tcc01.cc);
			assert(msg->u.rtpfb.u.tcc01.ccfb[0].seq == 5 && msg->u.rtpfb.u.tcc01.ccfb[0].received == 1 && msg->u.rtpfb.u.tcc01.ccfb[1].ato == 0);
			assert(msg->u.rtpfb.u.tcc01.ccfb[1].seq == 6 && msg->u.rtpfb.u.tcc01.ccfb[1].received == 0);
			assert(msg->u.rtpfb.u.tcc01.ccfb[2].seq == 7 && msg->u.rtpfb.u.tcc01.ccfb[2].received == 1 && msg->u.rtpfb.u.tcc01.ccfb[2].ato == 128);
			assert(msg->u.rtpfb.u.tcc01.ccfb[3].seq == 8 && msg->u.rtpfb.u.tcc01.ccfb[3].received == 1 && msg->u.rtpfb.u.tcc01.ccfb[3].ato == 64);
			assert(msg->u.rtpfb.u.tcc01.ccfb[4].seq == 9 && msg->u.rtpfb.u.tcc01.ccfb[4].received == 0);
			assert(msg->u.rtpfb.u.tcc01.ccfb[5].seq == 10 && msg->u.rtpfb.u.tcc01.ccfb[5].received == 0);
			assert(msg->u.rtpfb.u.tcc01.ccfb[6].seq == 11 && msg->u.rtpfb.u.tcc01.ccfb[6].received == 1 && msg->u.rtpfb.u.tcc01.ccfb[6].ato == 256);
			r = rtcp_rtpfb_tcc01_pack((uint16_t)msg->u.rtpfb.u.tcc01.begin, msg->u.rtpfb.u.tcc01.ccfb, msg->u.rtpfb.u.tcc01.count, msg->u.rtpfb.u.tcc01.timestamp, (uint8_t)msg->u.rtpfb.u.tcc01.cc, buffer, sizeof(buffer));
			assert(20 == r && 0 == memcmp(buffer, param, r));
			break;

		case 31: // test only
			assert(248 == msg->u.rtpfb.u.tcc01.begin && -5546573 == msg->u.rtpfb.u.tcc01.timestamp && 128 == msg->u.rtpfb.u.tcc01.count && 1 == msg->u.rtpfb.u.tcc01.cc);
			assert(msg->u.rtpfb.u.tcc01.ccfb[0].seq == 248 && msg->u.rtpfb.u.tcc01.ccfb[0].received == 1 && msg->u.rtpfb.u.tcc01.ccfb[0].ato == 33);
			assert(msg->u.rtpfb.u.tcc01.ccfb[1].seq == 249 && msg->u.rtpfb.u.tcc01.ccfb[1].received == 0);
			assert(msg->u.rtpfb.u.tcc01.ccfb[125].seq == 373 && msg->u.rtpfb.u.tcc01.ccfb[125].received == 1 && msg->u.rtpfb.u.tcc01.ccfb[125].ato == -50);
			assert(msg->u.rtpfb.u.tcc01.ccfb[126].seq == 374 && msg->u.rtpfb.u.tcc01.ccfb[126].received == 0);
			assert(msg->u.rtpfb.u.tcc01.ccfb[127].seq == 375 && msg->u.rtpfb.u.tcc01.ccfb[127].received == 1 && msg->u.rtpfb.u.tcc01.ccfb[127].ato == 65);
			r = rtcp_rtpfb_tcc01_pack((uint16_t)msg->u.rtpfb.u.tcc01.begin, msg->u.rtpfb.u.tcc01.ccfb, msg->u.rtpfb.u.tcc01.count, msg->u.rtpfb.u.tcc01.timestamp, (uint8_t)msg->u.rtpfb.u.tcc01.cc, buffer, sizeof(buffer));
			assert(20 == r && 0 == memcmp(buffer, param, r));
			break;

		case RTCP_RTPFB_CCFB:
			assert(0);
			break;

		default:
			break;
		}
		break;

	default:
		assert(0);
	}
}

static void rtcp_rtpfb_nack_test(void)
{
	//rtcp_nack_t* nack;
	//const uint8_t data[] = { 0x81, 0xcd, 0x00, 0x08, 0x84, 0x68, 0xc2, 0x4c, 0x84, 0x68, 0xc2, 0x4c, 0x03, 0x27, 0x7f, 0xff, 0x03, 0x38, 0xff, 0xf7, 0x03, 0x49, 0xff, 0xff, 0x03, 0x5a, 0xff, 0xff, 0x03, 0x6b, 0x7f, 0xbf, 0x03, 0x7c, 0x00, 0x0f };
	//assert(0 == rtcp_rtpfb_nack_unpack(NULL, NULL, 0, 0, data, sizeof(data)));
	//assert(nack[0].pid == 807 && nack[0].blp == 0x7fff);
	//assert(nack[1].pid == 824 && nack[1].blp == 0xfff7);
	//assert(nack[2].pid == 841 && nack[2].blp == 0xffff);
	//assert(nack[3].pid == 858 && nack[3].blp == 0xffff);
	//assert(nack[4].pid == 875 && nack[4].blp == 0x7fbf);
	//assert(nack[5].pid == 892 && nack[5].blp == 0x000f);

	const uint8_t data[] = { 0x02, 0x77, 0x80, 0x28, 0x02, 0x88, 0x00, 0x21, 0x02, 0x9a, 0x00, 0x08, 0x02, 0xb2, 0x20, 0x00, 0x02, 0xde, 0x00, 0x25, 0x02, 0xf5, 0x11, 0x00, 0x03, 0x09, 0x00, 0x00, 0x03, 0x3a, 0x00, 0x02, 0x03, 0x61, 0x00, 0x00, 0x03, 0x72, 0x03, 0x00, 0x03, 0x8b, 0x04, 0x00, 0x03, 0xa3, 0x00, 0x00, 0x03, 0xc3, 0x00, 0x6d };
	
	struct rtcp_msg_t msg;
	struct rtp_context rtp;
	rtp.handler.on_rtcp = rtcp_on_rtpfb_test;
	rtp.cbparam = (void*)data;

	msg.type = (RTCP_RTPFB_NACK << 8) | RTCP_RTPFB;
	assert(0 == rtcp_rtpfb_nack_unpack(&rtp, NULL, &msg, data, sizeof(data)));
}

static void rtcp_rtpfb_tmmbr_test(void)
{
	const uint8_t data[] = { 0x23, 0x45, 0x67, 0x89, 0x0a, 0x61, 0x61, 0xfe };

	struct rtcp_msg_t msg;
	struct rtp_context rtp;
	rtp.handler.on_rtcp = rtcp_on_rtpfb_test;
	rtp.cbparam = (void*)data;

	msg.type = (RTCP_RTPFB_TMMBR << 8) | RTCP_RTPFB;
	assert(0 == rtcp_rtpfb_tmmbr_unpack(&rtp, NULL, &msg, data, sizeof(data)));
}

static void rtcp_rtpfb_tmmbn_test(void)
{
	const uint8_t data[] = { 0x23, 0x45, 0x67, 0x89, 0x0a, 0x61, 0x61, 0xfe };

	struct rtcp_msg_t msg;
	struct rtp_context rtp;
	rtp.handler.on_rtcp = rtcp_on_rtpfb_test;
	rtp.cbparam = (void*)data;

	msg.type = (RTCP_RTPFB_TMMBN << 8) | RTCP_RTPFB;
	assert(0 == rtcp_rtpfb_tmmbn_unpack(&rtp, NULL, &msg, data, sizeof(data)));
}

static void rtcp_rtpfb_tcc01_test(void)
{
	//rtcp_ccfb_t* ccfb;
	//const uint8_t data[] = { 0x8f, 0xcd, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x84, 0x68, 0xc2, 0x49, 0x23, 0x8b, 0x01, 0x8b, 0x3e, 0x08, 0x49, 0x09, 0xd9, 0x00, 0x00, 0x0e, 0xa0, 0x00, 0x00, 0x1c, 0xa0, 0x00, 0x00, 0xe6, 0xa0, 0x00, 0x00, 0x49, 0x20, 0x01, 0x2c, 0xff, 0xf0, 0x0c, 0x44, 0x94, 0x8c, 0x50, 0x00, 0x00 };
	//assert(0 == rtcp_rtpfb_tcc01_unpack(NULL, NULL, 0, 0, data, sizeof(data)));
	//assert(395 == num && ccfb[0].seq == 9099 && ccfb[0].received && ccfb[1].ecn == 0x01 && ccfb[0].ato == 11);
	//assert(cfb[1].seq == 9100 && ccfb[1].received && ccfb[1].ecn == 0x02 && ccfb[1].ato == -4);

	const uint8_t data[] = { 0x06, 0xe7, 0x00, 0x65, 0x3e, 0xc6, 0x50, 0x2a, 0x9a, 0xff, 0x20, 0x16, 0x97, 0x68, 0xbc, 0xab, 0xa7, 0xfe, 0x20, 0x12, 0xc1, 0x50, 0x54, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01 };
	struct rtcp_msg_t msg;
	struct rtp_context rtp;
	rtp.handler.on_rtcp = rtcp_on_rtpfb_test;
	rtp.cbparam = (void*)data;
	msg.type = (RTCP_RTPFB_TCC01 << 8) | RTCP_RTPFB;
	assert(0 == rtcp_rtpfb_tcc01_unpack(&rtp, NULL, &msg, data, sizeof(data)));

	// 11-768ms, 8-512ms, 7-448ms, 5-320ms
	const uint8_t data2[] = { 0x00, 0x05, 0x00, 0x07, 0x00, 0x00, 0x05, 0x00, 0xd2, 0x82, 0x00, 0x02, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03 };
	rtp.cbparam = (void*)data2;
	msg.type = (30 << 8) | RTCP_RTPFB;
	assert(0 == rtcp_rtpfb_tcc01_unpack(&rtp, NULL, &msg, data2, sizeof(data2)));

	// 248-33ms, 373-(-50)ms, 375-65ms, timestamp: -5546573
	const uint8_t data3[] = { 0x00, 0xf8, 0x00, 0x80, 0xab, 0x5d, 0xb3, 0x01, 0xa0, 0x00, 0x00, 0x6f, 0xe2, 0x00, 0x84, 0xff, 0x38, 0x01, 0x04, 0x01 };
	rtp.cbparam = (void*)data3;
	msg.type = (31 << 8) | RTCP_RTPFB;
	assert(0 == rtcp_rtpfb_tcc01_unpack(&rtp, NULL, &msg, data3, sizeof(data3)));
}

void rtcp_rtpfb_test(void)
{
	rtcp_rtpfb_nack_test();
	rtcp_rtpfb_tmmbr_test();
	rtcp_rtpfb_tmmbn_test();
	rtcp_rtpfb_tcc01_test();
}
#endif
