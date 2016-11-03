// RFC3550 6.7 APP: Application-Defined RTCP Packet

#include "rtp-internal.h"
#include "rtp-util.h"

void rtcp_app_unpack(struct rtp_context *ctx, rtcp_header_t *header, const unsigned char* ptr)
{
	struct rtcp_msg_t msg;
	struct rtp_member *member;

	assert(header->length*4 >= 8);
	if(header->length < 8) // RTCP header + SSRC + name
		return;

	msg.type = RTCP_MSG_APP;
	msg.u.app.ssrc = nbo_r32(ptr);

	member = rtp_member_fetch(ctx, msg.u.app.ssrc);
	if(!member) return; // error	

	memcpy(msg.u.app.name, ptr+4, 4);

	if(header->length > 8)
	{
		msg.u.app.data = (void*)(ptr + 8);
		msg.u.app.bytes = header->length * 4 - 8;
	}
	else
	{
		msg.u.app.data = NULL;
		msg.u.app.bytes = 0;
	}

	ctx->handler.on_rtcp(ctx->cbparam, &msg);
}

size_t rtcp_app_pack(struct rtp_context *ctx, unsigned char* ptr, size_t bytes, const char name[4], const void* app, size_t len)
{
	rtcp_header_t header;

	if(bytes >= 8 + (len+3)/4*4)
	{
		header.v = 2;
		header.p = 0;
		header.pt = RTCP_APP;
		header.rc = 0;
		header.length = (uint16_t)(2+(len+3)/4);
		nbo_write_rtcp_header(ptr, &header);

		nbo_w32(ptr+4, ctx->self->ssrc);
		memcpy(ptr+8, name, 4);

		if(len > 0)
			memcpy(ptr+12, app, len);
	}

	return 8 + (len+3)/4*4;
}
