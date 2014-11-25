// RFC3550 6.6 BYE: Goodbye RTCP Packet

#include "rtp-internal.h"
#include "rtp-util.h"

void rtcp_bye_unpack(struct rtp_context *ctx, rtcp_header_t *header, const unsigned char* ptr)
{
	uint32_t i;
	struct rtcp_msg_t msg;

	assert(header->length >= header->rc * 4);
	if(header->rc < 1)
		return; // A count value of zero is valid, but useless (p43)

	msg.type = RTCP_MSG_BYE;
	if(header->length * 4 >= header->rc * 4 + 4)
	{
		msg.u.bye.bytes = (size_t)*(ptr + header->rc * 4);
		msg.u.bye.reason = (void*)(ptr + header->rc * 4 + 1);
		assert(msg.u.bye.bytes + 3 + header->rc*4 <= header->length * 4);
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

size_t rtcp_bye_pack(struct rtp_context *ctx, unsigned char* ptr, size_t bytes)
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
