// RFC3640 RTP Payload Format for Transport of MPEG-4 Elementary Streams

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-helper.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <errno.h>

static int rtp_decode_mpeg4_generic(void* p, const void* packet, int bytes)
{
	int i, size;
	int au_size;
	int au_numbers;
	int au_header_length;
	const uint8_t *ptr, *pau, *pend;
	struct rtp_packet_t pkt;
	struct rtp_payload_helper_t *helper;

	helper = (struct rtp_payload_helper_t *)p;
	if (!helper || 0 != rtp_packet_deserialize(&pkt, packet, bytes) || pkt.payloadlen < 4)
		return -EINVAL;

	rtp_payload_check(helper, &pkt);

	if (helper->lost)
	{
		assert(0 == helper->size);
		return 0; // packet discard
	}

	// save payload
	ptr = (const uint8_t *)pkt.payload;
	pend = ptr + pkt.payloadlen;

	// AU-headers-length
	au_header_length = (ptr[0] << 8) + ptr[1];
	au_header_length = (au_header_length + 7) / 8; // bit -> byte

	if (ptr + au_header_length /*AU-size*/ > pend || au_header_length < 2)
	{
		assert(0);
		helper->size = 0;
		helper->lost = 1;
		helper->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;
		return -1; // invalid packet
	}

	// 3.3.6. High Bit-rate AAC
	// SDP fmtp: sizeLength=13; indexLength=3; indexDeltaLength=3;
	au_size = 2; // only AU-size
	au_numbers = au_header_length / au_size;
	assert(0 == au_header_length % au_size);
	ptr += 2; // skip AU headers length section 2-bytes
	pau = ptr + au_header_length; // point to Access Unit

	for (i = 0; i < au_numbers; i++)
	{
		size = (ptr[0] << 8) | (ptr[1] & 0xF8);
		size = size >> 3; // bit -> byte
		if (pau + size > pend)
		{
			assert(0);
			helper->size = 0;
			helper->lost = 1;
			helper->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;
			return -1; // invalid packet
		}

		// TODO: add ADTS/ASC ???
		pkt.payload = pau;
		pkt.payloadlen = size;
		rtp_payload_write(helper, &pkt);

		ptr += au_size;
		pau += size;

		if (au_numbers > 1 || pkt.rtp.m)
		{
			rtp_payload_onframe(helper);
		}
	}

	return helper->lost ? 0 : 1; // packet handled
}

struct rtp_payload_decode_t *rtp_mpeg4_generic_decode()
{
	static struct rtp_payload_decode_t unpacker = {
		rtp_payload_helper_create,
		rtp_payload_helper_destroy,
		rtp_decode_mpeg4_generic,
	};

	return &unpacker;
}
