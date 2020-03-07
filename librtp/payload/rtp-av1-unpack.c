// https://aomediacodec.github.io/av1-rtp-spec/#41-rtp-header-usage

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-helper.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <errno.h>

static inline const uint8_t* leb128(const uint8_t* data, size_t bytes, int64_t* size)
{
	size_t i;
	for (*size = i = 0; i < 8 && i < bytes;)
	{
		*size |= ((int64_t)(data[i] & 0x7F)) << (i * 7);
		if (0 == (data[i++] & 0x80))
			break;
	}
	return data + i;
}

/*
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X|  CC   |M|     PT      |       sequence number         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           synchronization source (SSRC) identifier            |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|            contributing source (CSRC) identifiers             |
|                             ....                              |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|         0x100         |  0x0  |       extensions length       |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|   0x1(ID)     |  hdr_length   |                               |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+                               |
|                                                               |
|          dependency descriptor (hdr_length #octets)           |
|                                                               |
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               | Other rtp header extensions...|
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
| AV1 aggr hdr  |                                               |
+-+-+-+-+-+-+-+-+                                               |
|                                                               |
|                   Bytes 2..N of AV1 payload                   |
|                                                               |
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               :    OPTIONAL RTP padding       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtp_decode_av1(void* p, const void* packet, int bytes)
{
	int64_t size;
	uint8_t z, y, w, n;
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

	// AV1 aggregation header
	/*
	0 1 2 3 4 5 6 7
	+-+-+-+-+-+-+-+-+
	|Z|Y| W |N|-|-|-|
	+-+-+-+-+-+-+-+-+
	*/
	z = ptr[0] & 0x80;
	y = ptr[0] & 0x40;
	w = (ptr[0] & 0x30) >> 4;
	n = ptr[0] & 0x08;
	ptr++;

	if (n)
	{
		rtp_payload_onframe(helper);
	}

	// skip fragment frame OBU size
	if (z)
		ptr = leb128(ptr, pend - ptr, &size);

	if (ptr >= pend)
	{
		assert(0);
		helper->size = 0;
		helper->lost = 1;
		helper->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;
		return -1; // invalid packet
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

struct rtp_payload_decode_t *rtp_av1_decode()
{
	static struct rtp_payload_decode_t unpacker = {
		rtp_payload_helper_create,
		rtp_payload_helper_destroy,
		rtp_decode_av1,
	};

	return &unpacker;
}
