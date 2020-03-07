// RTP Payload Format for VP9 Video draft-ietf-payload-vp9-03

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
|             VP9 payload descriptor (integer #octets)          |
:                                                               :
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               :  VP9 pyld hdr |               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               |
|                                                               |
+                                                               |
:                    Bytes 2..N of VP9 payload                  :
|                                                               |
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               :      OPTIONAL RTP padding     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtp_decode_vp9(void* p, const void* packet, int bytes)
{
	uint8_t pictureid_present;
	uint8_t inter_picture_predicted_layer_frame;
	uint8_t layer_indices_preset;
	uint8_t flex_mode;
	uint8_t start_of_layer_frame;
	uint8_t end_of_layer_frame;
	uint8_t scalability_struct_data_present;

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

	// VP9 payload descriptor
	/*
	 0 1 2 3 4 5 6 7
	+-+-+-+-+-+-+-+-+
	|I|P|L|F|B|E|V|-| (REQUIRED)
	+-+-+-+-+-+-+-+-+
	*/
	pictureid_present = ptr[0] & 0x80;
	inter_picture_predicted_layer_frame = ptr[0] & 0x40;
	layer_indices_preset = ptr[0] & 0x20;
	flex_mode = ptr[0] & 0x10;
	start_of_layer_frame = ptr[0] & 0x80;
	end_of_layer_frame = ptr[0] & 0x04;
	scalability_struct_data_present = ptr[0] & 0x02;
	ptr++;

	if (pictureid_present && ptr < pend)
	{
		//    +-+-+-+-+-+-+-+-+
		// I: |M|  PICTURE ID | (RECOMMENDED)
		//    +-+-+-+-+-+-+-+-+
		// M: |  EXTENDED PID | (RECOMMENDED)
		//    +-+-+-+-+-+-+-+-+
		uint16_t picture_id;
		picture_id = ptr[0] & 0x7F;
		if ((ptr[0] & 0x80) && ptr + 1 < pend)
		{
			picture_id = (ptr[0] << 8) | ptr[1];
			ptr++;
		}
		ptr++;
	}

	if (layer_indices_preset && ptr < pend)
	{
		//	  +-+-+-+-+-+-+-+-+
		// L: | T | U | S | D | (CONDITIONALLY RECOMMENDED)
		//    +-+-+-+-+-+-+-+-+
		//    |   TL0PICIDX   | (CONDITIONALLY REQUIRED)
		//    +-+-+-+-+-+-+-+-+
		
		// ignore Layer indices
		if (0 == flex_mode)
			ptr++; // TL0PICIDX
		ptr++;
	}

	if (inter_picture_predicted_layer_frame && flex_mode && ptr < pend)
	{
		//      +-+-+-+-+-+-+-+-+							-\
		// P,F: |    P_DIFF   |N| (CONDITIONALLY REQUIRED)  - up to 3 times
		//      +-+-+-+-+-+-+-+-+							-/

		// ignore Reference indices
		if (ptr[0] & 0x01)
		{
			if ((ptr[1] & 0x01) && ptr + 1 < pend)
				ptr++;
			ptr++;
		}
		ptr++;
	}

	if (scalability_struct_data_present && ptr < pend)
	{
		/*
			+-+-+-+-+-+-+-+-+
		V:	| N_S |Y|G|-|-|-|
			+-+-+-+-+-+-+-+-+			 -\
		Y:	|     WIDTH		| (OPTIONAL) .
			+				+			 .
			|				| (OPTIONAL) .
			+-+-+-+-+-+-+-+-+			 . - N_S + 1 times
			|     HEIGHT	| (OPTIONAL) .
			+				+			 .
			|				| (OPTIONAL) .
			+-+-+-+-+-+-+-+-+			 -/				-\
		G:	|      N_G      | (OPTIONAL)
			+-+-+-+-+-+-+-+-+							-\
		N_G:|  T  |U| R |-|-| (OPTIONAL)				.
			+-+-+-+-+-+-+-+-+			 -\				. - N_G times
			|     P_DIFF    | (OPTIONAL) .  - R times	.
			+-+-+-+-+-+-+-+-+			 -/				-/
		*/
		uint8_t N_S, Y, G;
		N_S = ((ptr[0] >> 5) & 0x07) + 1;
		Y = ptr[0] & 0x10;
		G = ptr[0] & 0x80;
		ptr++;

		if (Y)
		{
			ptr += N_S * 4;
		}

		if (G && ptr < pend)
		{
			uint8_t i;
			uint8_t N_G = ptr[0];
			ptr++;

			for (i = 0; i < N_G && ptr < pend; i++)
			{
				uint8_t j;
				uint8_t R;

				R = (ptr[0] >> 2) & 0x03;
				ptr++;

				for (j = 0; j < R && ptr < pend; j++)
				{
					// ignore P_DIFF
					ptr++;
				}
			}
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

	if (start_of_layer_frame)
	{
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

struct rtp_payload_decode_t *rtp_vp9_decode()
{
	static struct rtp_payload_decode_t unpacker = {
		rtp_payload_helper_create,
		rtp_payload_helper_destroy,
		rtp_decode_vp9,
	};

	return &unpacker;
}
