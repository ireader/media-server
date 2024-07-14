// RFC3550 6.6 BYE: Goodbye RTCP Packet

#include "rtp-internal.h"
#include "rtp-util.h"

void rtcp_bye_unpack(struct rtp_context *ctx, const rtcp_header_t *header, const uint8_t* ptr, size_t bytes)
{
	uint32_t i;
	struct rtcp_msg_t msg;

	assert(bytes >= header->rc * 4);
	if(header->rc < 1 || header->rc * 4 > bytes)
		return; // A count value of zero is valid, but useless (p43)

	msg.ssrc = nbo_r32(ptr);
	msg.type = RTCP_BYE;

	rtp_member_list_delete(ctx->members, msg.ssrc);
	rtp_member_list_delete(ctx->senders, msg.ssrc);

	if(bytes > header->rc * 4 + 1)
	{
		msg.u.bye.bytes = ptr[header->rc * 4];
		msg.u.bye.reason = ptr + header->rc * 4 + 1;

		if (1 + msg.u.bye.bytes + header->rc * 4 > bytes)
		{
			assert(0);
			msg.u.bye.bytes = 0;
			msg.u.bye.reason = NULL;
		}
	}
	else
	{
		msg.u.bye.bytes = 0;
		msg.u.bye.reason = NULL;
	}

	ctx->handler.on_rtcp(ctx->cbparam, &msg);

	// other SSRC/CSRC
	for (i = 0; i < header->rc /*source count*/; i++)
	{
		msg.ssrc = nbo_r32(ptr + 4 + i * 4);
		rtp_member_list_delete(ctx->members, msg.ssrc);
		rtp_member_list_delete(ctx->senders, msg.ssrc);
		ctx->handler.on_rtcp(ctx->cbparam, &msg);
	}
}

int rtcp_bye_pack(struct rtp_context *ctx, uint8_t* ptr, int bytes)
{
	rtcp_header_t header;

	if(bytes < 8)
		return 8;

	header.v = 2;
	header.p = 0;
	header.pt = RTCP_BYE;
	header.rc = 1; // self only
	header.length = 1;
	nbo_write_rtcp_header(ptr, &header);

	nbo_w32(ptr+4, ctx->self->ssrc);

	assert(8 == (header.length+1)*4);
	return 8;
}
