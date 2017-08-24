/// RFC3016 RTP Payload Format for MPEG-4 Audio/Visual Streams
/// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-helper.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <errno.h>

static int rtp_decode_mp4a_latm(void* p, const void* packet, int bytes)
{
	int len;
	const uint8_t *ptr, *pend;
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
	if (0 == helper->size)
	{
		ptr = (const uint8_t *)pkt.payload;
		for (pend = ptr + pkt.payloadlen; ptr < pend; ptr += len)
		{
			// ISO/IEC 14496-3:200X(E)
			// Table 1.44 ¨C Syntax of PayloadLengthInfo() (p84)
			// Table 1.45 ¨C Syntax of PayloadMux()
			for (len = 0; ptr < pend; ptr++)
			{
				len += *ptr;
				if (255 != *ptr)
				{
					++ptr;
					break;
				}
			}

			if (ptr + len > pend)
			{
				assert(0);
				helper->size = 0;
				helper->lost = 1;
				helper->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;
				return -1; // invalid packet
			}

			// TODO: add ADTS/ASC ???
			pkt.payload = ptr;
			pkt.payloadlen = len;
			rtp_payload_write(helper, &pkt);

			if (ptr + len < pend || pkt.rtp.m)
			{
				rtp_payload_onframe(helper);
			}
		}
	}
	else
	{
		// RFC6416 6.3. Fragmentation of MPEG-4 Audio Bitstream (p17)
		// It is RECOMMENDED to put one audioMuxElement in each RTP packet. If
		// the size of an audioMuxElement can be kept small enough that the size
		// of the RTP packet containing it does not exceed the size of the Path
		// MTU, this will be no problem.If it cannot, the audioMuxElement
		// SHALL be fragmented and spread across multiple packets.
		rtp_payload_write(helper, &pkt);
		if (pkt.rtp.m)
		{
			rtp_payload_onframe(helper);
		}
	}

	return helper->lost ? 0 : 1; // packet handled
}

struct rtp_payload_decode_t *rtp_mp4a_latm_decode()
{
	static struct rtp_payload_decode_t unpacker = {
		rtp_payload_helper_create,
		rtp_payload_helper_destroy,
		rtp_decode_mp4a_latm,
	};

	return &unpacker;
}
