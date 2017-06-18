// RFC3550 6.6 BYE: Goodbye RTCP Packet

#include "rtp-internal.h"
#include "rtp-util.h"

void rtcp_bye_unpack(struct rtp_context *ctx, rtcp_header_t *header, const uint8_t* ptr)
{
	uint32_t i;
	struct rtcp_msg_t msg;

	assert(header->length * 4 >= header->rc * 4);
	if(header->rc < 1 || header->rc > header->length)
		return; // A count value of zero is valid, but useless (p43)

	msg.type = RTCP_MSG_BYE;
	if(header->length * 4 > header->rc * 4)
	{
		msg.u.bye.bytes = ptr[header->rc * 4];
		msg.u.bye.reason = ptr + header->rc * 4 + 1;

		if (1 + msg.u.bye.bytes + header->rc * 4 > header->length * 4)
		{
			assert(0);
			return; // error
		}
	}
	else
	{
		msg.u.bye.bytes = 0;
		msg.u.bye.reason = NULL;
	}

	for(i = 0; i < header->rc; i++)
	{
		msg.u.bye.ssrc = nbo_r32(ptr + i * 4);
		rtp_member_list_delete(ctx->members, msg.u.bye.ssrc);
		rtp_member_list_delete(ctx->senders, msg.u.bye.ssrc);

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
