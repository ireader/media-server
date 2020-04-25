/// RFC2250 3. Encapsulation of MPEG Elementary Streams (p4)

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-helper.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define MPEG2VIDEO_EXTENSION_HEADER 0x04

static int rtp_decode_mpeg2es(void* p, const void* packet, int bytes)
{
	int n;
	struct rtp_packet_t pkt;
	struct rtp_payload_helper_t *helper;

	helper = (struct rtp_payload_helper_t *)p;
	if (!helper || 0 != rtp_packet_deserialize(&pkt, packet, bytes) || pkt.payloadlen < 4)
		return -EINVAL;

	if (RTP_PAYLOAD_MP3 != pkt.rtp.pt && RTP_PAYLOAD_MPV != pkt.rtp.pt)
	{
		assert(0);
		return -EINVAL;
	}

	rtp_payload_check(helper, &pkt);

	// save payload
	if (!helper->lost)
	{
		n = 4; // skip 3.4 MPEG Video-specific header
		if (RTP_PAYLOAD_MPV == pkt.rtp.pt && (((uint8_t*)pkt.payload)[4] & MPEG2VIDEO_EXTENSION_HEADER))
			n += 4; // 3.4.1 MPEG-2 Video-specific header extension

		assert(pkt.payloadlen > 4);
		if (pkt.payload && pkt.payloadlen > n)
		{
			pkt.payload = (uint8_t*)pkt.payload + n;
			pkt.payloadlen -= n;
			rtp_payload_write(helper, &pkt);
		}
	}

	// M bit: For video, set to 1 on packet containing MPEG frame end code, 0 otherwise.
	//        For audio, set to 1 on first packet of a "talk-spurt," 0 otherwise.
	if (pkt.rtp.m && RTP_PAYLOAD_MPV == pkt.rtp.pt)
	{
		rtp_payload_onframe(helper);
	}

	return helper->lost ? 0 : 1; // packet handled
}

struct rtp_payload_decode_t *rtp_mpeg1or2es_decode()
{
	static struct rtp_payload_decode_t unpacker = {
		rtp_payload_helper_create,
		rtp_payload_helper_destroy,
		rtp_decode_mpeg2es,
	};

	return &unpacker;
}
