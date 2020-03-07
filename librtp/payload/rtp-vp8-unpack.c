// RFC7731 RTP Payload Format for VP8 Video

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-helper.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <errno.h>

/*
 0               1               2               3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X|   CC  |M|      PT     |         sequence number       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            synchronization source (SSRC) identifier           |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|             contributing source (CSRC) identifiers            |
|                              ....                             |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|             VP8 payload descriptor (integer #octets)          |
:                                                               :
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               : VP8 payload header (3 octets) |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| VP8 pyld hdr  :                                               |
+-+-+-+-+-+-+-+-+                                               |
:                   Octets 4..N of VP8 payload                  :
|                                                               |
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               :      OPTIONAL RTP padding     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtp_decode_vp8(void* p, const void* packet, int bytes)
{
	uint8_t extended_control_bits;
	uint8_t start_of_vp8_partition;
	uint8_t PID;
	const uint8_t *ptr, *pend;
	struct rtp_packet_t pkt;
	struct rtp_payload_helper_t *helper;

	helper = (struct rtp_payload_helper_t *)p;
	if (!helper || 0 != rtp_packet_deserialize(&pkt, packet, bytes) || pkt.payloadlen < 1)
		return -EINVAL;

	rtp_payload_check(helper, &pkt);

	if (helper->lost)
	{
		assert(0 == helper->size);
		return 0; // packet discard
	}

	ptr = (const uint8_t *)pkt.payload;
	pend = ptr + pkt.payloadlen;

	// VP8 payload descriptor
	extended_control_bits = ptr[0] & 0x80;
	start_of_vp8_partition = ptr[0] & 0x10;
	PID = ptr[0] & 0x0f;
	ptr++;

	if (extended_control_bits && ptr < pend)
	{
		/*
			 0 1 2 3 4 5 6 7
			+-+-+-+-+-+-+-+-+
			|X|R|N|S|R| PID | (REQUIRED)
			+-+-+-+-+-+-+-+-+
		X:  |I|L|T|K|   RSV | (OPTIONAL)
			+-+-+-+-+-+-+-+-+
		I:  |M|  PictureID  | (OPTIONAL)
			+-+-+-+-+-+-+-+-+
			|   PictureID   |
			+-+-+-+-+-+-+-+-+
		L:  |   TL0PICIDX   | (OPTIONAL)
			+-+-+-+-+-+-+-+-+
		T/K:|TID|Y|  KEYIDX | (OPTIONAL)
			+-+-+-+-+-+-+-+-+
		*/
		uint8_t pictureid_present;
		uint8_t tl0picidx_present;
		uint8_t tid_present;
		uint8_t keyidx_present;

		pictureid_present = ptr[0] & 0x80;
		tl0picidx_present = ptr[0] & 0x40;
		tid_present = ptr[0] & 0x20;
		keyidx_present = ptr[0] & 0x10;
		ptr++;

		if (pictureid_present && ptr < pend)
		{
			uint16_t picture_id;
			picture_id = ptr[0] & 0x7F;
			if ((ptr[0] & 0x80) && ptr + 1 < pend)
			{
				picture_id = (ptr[0] << 8) | ptr[1];
				ptr++;
			}
			ptr++;
		}

		if (tl0picidx_present && ptr < pend)
		{
			// ignore temporal level zero index
			ptr++;
		}

		if ((tid_present || keyidx_present) && ptr < pend)
		{
			// ignore KEYIDX
			ptr++;
		}
	}

	if (ptr >= pend)
	{
		assert(0);
		helper->size = 0;
		helper->lost = 1;
		helper->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;
		return -1; // invalid packet
	}

	// VP8 payload header (3 octets)
	if (start_of_vp8_partition)
	{
		/*
		0 1 2 3 4 5 6 7
		+-+-+-+-+-+-+-+-+
		|Size0|H| VER |P|
		+-+-+-+-+-+-+-+-+
		| Size1 |
		+-+-+-+-+-+-+-+-+
		| Size2 |
		+-+-+-+-+-+-+-+-+
		*/
		// P: Inverse key frame flag. When set to 0, the current frame is a key
		//    frame. When set to 1, the current frame is an interframe.
		//    Defined in [RFC6386]
		int keyframe;
		keyframe = ptr[0] & 0x01;

		// new frame begin
		rtp_payload_onframe(helper);
	}

	pkt.payload = ptr;
	pkt.payloadlen = (int)(pend - ptr);
	rtp_payload_write(helper, &pkt);

	if (pkt.rtp.m)
	{
		rtp_payload_onframe(helper);
	}

	return 1; // packet handled
}

struct rtp_payload_decode_t *rtp_vp8_decode()
{
	static struct rtp_payload_decode_t unpacker = {
		rtp_payload_helper_create,
		rtp_payload_helper_destroy,
		rtp_decode_vp8,
	};

	return &unpacker;
}
