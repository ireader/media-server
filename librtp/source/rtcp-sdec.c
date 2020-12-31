// RFC3550 6.5 SDES: Source Description RTCP Packet

#include "rtp-internal.h"
#include "rtp-util.h"

void rtcp_sdes_unpack(struct rtp_context *ctx, rtcp_header_t *header, const uint8_t* ptr)
{
	uint32_t i;
	uint32_t ssrc;
	struct rtp_member *member;
	const unsigned char *p, *end;

	p = ptr;
	end = ptr + header->length * 4;
	assert(header->length >= header->rc);

	for(i = 0; i < header->rc && p + 8 /*4-ssrc + 1-PT*/ <= end; i++)
	{
		ssrc = nbo_r32(p);
		member = rtp_member_fetch(ctx, ssrc);
		if(!member)
			continue;

		p += 4;
		while(p + 2 <= end && RTCP_SDES_END != p[0] /*PT*/)
		{
			rtcp_sdes_item_t item;
			item.pt = p[0];
			item.len = p[1];
			item.data = (unsigned char*)(p+2);
			if (p + 2 + item.len > end)
			{
				assert(0);
				return; // error
			}

			switch(item.pt)
			{
			case RTCP_SDES_CNAME:
			case RTCP_SDES_NAME:
			case RTCP_SDES_EMAIL:
			case RTCP_SDES_PHONE:
			case RTCP_SDES_LOC:
			case RTCP_SDES_TOOL:
			case RTCP_SDES_NOTE:
				rtp_member_setvalue(member, item.pt, item.data, item.len);
				break;

			case RTCP_SDES_PRIVATE:
				assert(0);
				break;

			default:
				assert(0);
			}

			// RFC3550 6.5 SDES: Source Description RTCP Packet
			// Items are contiguous, i.e., items are not individually padded to a 32-bit boundary. 
			// Text is not null terminated because some multi-octet encodings include null octets.
			p += 2 + item.len;
		}

		// RFC3550 6.5 SDES: Source Description RTCP Packet
		// The list of items in each chunk must be terminated by one or more null octets,
		// the first of which is interpreted as an item type of zero to denote the end of the list.
		// No length octet follows the null item type octet, 
		// but additional null octets must be included if needed to pad until the next 32-bit boundary.
		// offset sizeof(SSRC) + sizeof(chunk type) + sizeof(chunk length)
		p = (const unsigned char *)((p - (const unsigned char *)0 + 3) / 4 * 4);
	}
}

static size_t rtcp_sdes_append_item(unsigned char *ptr, size_t bytes, rtcp_sdes_item_t *sdes)
{
	assert(sdes->data);
	if(bytes >= (size_t)sdes->len+2)
	{
		ptr[0] = sdes->pt;
		ptr[1] = sdes->len;
		memcpy(ptr+2,sdes->data, sdes->len);
	}

	return sdes->len+2;
}

int rtcp_sdes_pack(struct rtp_context *ctx, uint8_t* ptr, int bytes)
{
	int n;
	rtcp_header_t header;

	// must have CNAME
	if(!ctx->self->sdes[RTCP_SDES_CNAME].data)
		return 0;

	header.v = 2;
	header.p = 0;
	header.pt = RTCP_SDES;
	header.rc = 1; // self only
	header.length = 0;

	n = (int)rtcp_sdes_append_item(ptr+8, bytes-8, &ctx->self->sdes[RTCP_SDES_CNAME]);
	if(bytes < 8 + n)
		return 8 + n;

	// RFC3550 6.3.9 Allocation of Source Description Bandwidth (p29)
	// Every third interval (15 seconds), one extra item would be included in the SDES packet
	if(0 == ctx->rtcp_cycle % 3 && ctx->rtcp_cycle/3 > 0) // skip CNAME
	{
		assert(ctx->rtcp_cycle/3 < RTCP_SDES_PRIVATE);
		if(ctx->self->sdes[ctx->rtcp_cycle/3+1].data) // skip RTCP_SDES_END
		{
			n += rtcp_sdes_append_item(ptr+8+n, bytes-n-8, &ctx->self->sdes[ctx->rtcp_cycle/3+1]);
			if(n + 8 > bytes)
				return n + 8;
		}
	}

	ctx->rtcp_cycle = (ctx->rtcp_cycle+1) % 24; // 3 * SDES item number

	header.length = (uint16_t)((n+4+3)/4); // see 6.4.1 SR: Sender Report RTCP Packet
	nbo_write_rtcp_header(ptr, &header);
	nbo_w32(ptr+4, ctx->self->ssrc);

	return (header.length+1)*4;
}
