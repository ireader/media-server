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

#define N_MAX_OBU 255
#define N_RESERVED_OBU_SIZE_FIELD 2

struct rtp_decode_av1_obu_t
{
	int off;
	int len;
};

struct rtp_decode_av1_t
{
	struct rtp_payload_t handler;
	void* cbparam;

	int lost;
	uint16_t seq; // rtp seq
	uint32_t timestamp;

	int flags;

	struct
	{
		struct rtp_decode_av1_obu_t* arr;
		int num, cap;
	} obu;

	struct
	{
		uint8_t* ptr;
		int len, cap;
	} ptr;
};

static inline const uint8_t* leb128(const uint8_t* data, size_t bytes, int64_t* size)
{
	size_t i;
	for (*size = i = 0; i * 7 < 64 && i < bytes;)
	{
		*size |= ((int64_t)(data[i] & 0x7F)) << (i * 7);
		if (0 == (data[i++] & 0x80))
			break;
	}
	return data + i;
}

//static inline uint8_t* leb128_write(int64_t size, uint8_t* data, size_t bytes)
//{
//	size_t i;
//	assert(3 == bytes && size <= 0x1FFFFF);
//	for (i = 0; i * 7 < 64 && i < bytes; i++)
//	{
//		data[i] = (uint8_t)(size & 0x7F);
//		size >>= 7;
//		data[i] |= (size > 0 || i + 1 < bytes)? 0x80 : 0;
//	}
//	return data + i;
//}

static inline int leb128_write(int64_t size, uint8_t* data, int bytes)
{
	int i;
	for (i = 0; i * 7 < 64 && i < bytes;)
	{
		data[i] = (uint8_t)(size & 0x7F);
		size >>= 7;
		data[i++] |= size > 0 ? 0x80 : 0;
		if (0 == size)
			break;
	}
	return i;
}

static void* rtp_av1_unpack_create(struct rtp_payload_t* handler, void* param)
{
	struct rtp_decode_av1_t* unpacker;
	unpacker = (struct rtp_decode_av1_t*)calloc(1, sizeof(*unpacker));
	if (!unpacker)
		return NULL;

	memcpy(&unpacker->handler, handler, sizeof(unpacker->handler));
	unpacker->cbparam = param;
	unpacker->flags = -1;
	return unpacker;
}

static void rtp_av1_unpack_destroy(void* p)
{
	struct rtp_decode_av1_t* unpacker;
	unpacker = (struct rtp_decode_av1_t*)p;

	if (unpacker->obu.arr)
		free(unpacker->obu.arr);
	if (unpacker->ptr.ptr)
		free(unpacker->ptr.ptr);
#if defined(_DEBUG) || defined(DEBUG)
	memset(unpacker, 0xCC, sizeof(*unpacker));
#endif
	free(unpacker);
}

static int rtp_av1_unpack_obu_append(struct rtp_decode_av1_t* unpacker, const uint8_t* data, int bytes, int start)
{
	void* p;
	int size;
	int64_t n;
	uint8_t head;
	const uint8_t* pend;

	pend = data + bytes;
	size = unpacker->ptr.len + bytes + 9 /*obu_size*/ + 2 /*obu temporal delimiter*/;
	if (size > RTP_PAYLOAD_MAX_SIZE || size < 0 || bytes < 2)
		return -EINVAL;

	if (size >= unpacker->ptr.cap)
	{
		size += size / 4 > 16000 ? size / 4 : 16000;
		p = realloc(unpacker->ptr.ptr, size);
		if (!p)
		{
			//unpacker->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;
			unpacker->lost = 1;
			//unpacker->size = 0;
			return -ENOMEM;
		}	

		unpacker->ptr.ptr = (uint8_t*)p;
		unpacker->ptr.cap = size;
	}

	if (unpacker->obu.num + start >= unpacker->obu.cap)
	{
		if (unpacker->obu.cap >= N_MAX_OBU)
			return -E2BIG;
		p = realloc(unpacker->obu.arr, sizeof(struct rtp_decode_av1_obu_t) * (unpacker->obu.cap + 8));
		if (!p)
			return -ENOMEM;

		memset((struct rtp_decode_av1_obu_t*)p + unpacker->obu.cap, 0, sizeof(struct rtp_decode_av1_obu_t) * 8);
		unpacker->obu.arr = (struct rtp_decode_av1_obu_t*)p;
		unpacker->obu.cap += 8;
	}

	// add temporal delimiter obu
	//if (0 == unpacker->ptr.len)
	//{
	//	static const uint8_t av1_temporal_delimiter[] = { 0x12, 0x00 };
	//	assert(0 == unpacker->ptr.len);
	//	memcpy(unpacker->ptr.ptr, av1_temporal_delimiter, sizeof(av1_temporal_delimiter));
	//	unpacker->ptr.len += sizeof(av1_temporal_delimiter);
	//}

	if (start)
	{
		unpacker->obu.arr[unpacker->obu.num].off = unpacker->ptr.len;
		unpacker->obu.arr[unpacker->obu.num].len = 0;

		// obu_head
		head = *data++;
		unpacker->ptr.ptr[unpacker->ptr.len++] = head;
		if (head & 0x04) { // obu_extension_flag
			unpacker->ptr.ptr[unpacker->ptr.len++] = *data++;
		}

		if (head & 0x02) { // obu_has_size_field
			data = leb128(data, pend - data, &n);
			unpacker->ptr.len += leb128_write(n, unpacker->ptr.ptr + unpacker->ptr.len, unpacker->ptr.cap - unpacker->ptr.len);
		} else {
			unpacker->ptr.len += N_RESERVED_OBU_SIZE_FIELD; // obu_size
		}

		unpacker->obu.num++;
	}

	unpacker->obu.arr[unpacker->obu.num-1].len += (int)(intptr_t)(pend - data);

	// obu
	memcpy(unpacker->ptr.ptr + unpacker->ptr.len, data, pend - data);
	unpacker->ptr.len += (int)(intptr_t)(pend - data);
	return 0;
}

int rtp_av1_unpack_onframe(struct rtp_decode_av1_t* unpacker)
{
	int i, j, r, n;
	uint8_t* obu, *data, obu_size_field[9];
	int64_t len;
	r = 0;

	if (unpacker->obu.num < unpacker->obu.cap && unpacker->obu.arr[0].len > 0
#if !defined(RTP_ENABLE_COURRUPT_PACKET)
		&& 0 == unpacker->lost
#endif
		)
	{
		// write obu length
		for (i = 0; i < unpacker->obu.num; i++)
		{
			obu = unpacker->ptr.ptr + unpacker->obu.arr[i].off;
			data = obu + ((0x04 & obu[0]) ? 2 : 1);
			if (0x02 & obu[0]) { // obu_has_size_field
				assert(unpacker->lost || (leb128(data, 9, &len) && len == unpacker->obu.arr[i].len));
				continue;
			}

			obu[0] |= 0x02; // obu_has_size_field
			n = leb128_write(unpacker->obu.arr[i].len, obu_size_field, sizeof(obu_size_field));
			if (n != N_RESERVED_OBU_SIZE_FIELD) {
				memmove(data + n, data + N_RESERVED_OBU_SIZE_FIELD, unpacker->ptr.ptr + unpacker->ptr.len - (data + N_RESERVED_OBU_SIZE_FIELD));
				unpacker->ptr.len -= N_RESERVED_OBU_SIZE_FIELD - n;
				for (j = i + 1; j < unpacker->obu.num; j++)
				{
					assert(unpacker->obu.arr[j].off + n > N_RESERVED_OBU_SIZE_FIELD);
					unpacker->obu.arr[j].off = unpacker->obu.arr[j].off + n - N_RESERVED_OBU_SIZE_FIELD;
				}
			}
			memcpy(data, obu_size_field, n);
		}

		// previous packet done
		r = unpacker->handler.packet(unpacker->cbparam, unpacker->ptr.ptr, unpacker->ptr.len, unpacker->timestamp, unpacker->flags | (unpacker->lost ? RTP_PAYLOAD_FLAG_PACKET_CORRUPT : 0));

		// RTP_PAYLOAD_FLAG_PACKET_LOST: miss
		unpacker->flags &= ~RTP_PAYLOAD_FLAG_PACKET_LOST; // clear packet lost flag
	}

	// set packet lost flag on next frame
	if (unpacker->lost)
		unpacker->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;

	// new frame start
	unpacker->lost = 0;
	unpacker->ptr.len = 0;
	unpacker->obu.num = 0;
	memset(unpacker->obu.arr, 0, sizeof(struct rtp_decode_av1_obu_t) * unpacker->obu.cap);
	return r;
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
static int rtp_av1_unpack_input(void* p, const void* packet, int bytes)
{
	int lost;
	int64_t size;
	uint8_t z, y, w, n, i;
	const uint8_t *ptr, *pend;
	struct rtp_packet_t pkt;
	struct rtp_decode_av1_t* unpacker;

	unpacker = (struct rtp_decode_av1_t*)p;
	if (!unpacker || 0 != rtp_packet_deserialize(&pkt, packet, bytes) || pkt.payloadlen < 1)
		return -EINVAL;

	lost = 0;
	if (-1 == unpacker->flags)
	{
		unpacker->flags = 0;
		unpacker->seq = (uint16_t)(pkt.rtp.seq - 1); // disable packet lost
	}

	if ((uint16_t)pkt.rtp.seq != (uint16_t)(unpacker->seq + 1))
	{
		lost = 1;
		unpacker->flags = RTP_PAYLOAD_FLAG_PACKET_LOST;
		unpacker->ptr.len = 0; // discard previous packets
	}
	
	// check timestamp
	if (pkt.rtp.timestamp != unpacker->timestamp)
	{
		rtp_av1_unpack_onframe(unpacker);

		// lost:
		// 0 - packet lost before timestamp change
		// 1 - packet lost on timestamp changed, can't known losted packet is at old packet tail or new packet start, so two packets mark as packet lost
		if (0 != lost)
			unpacker->lost = lost;
	}
	unpacker->seq = (uint16_t)pkt.rtp.seq;
	unpacker->timestamp = pkt.rtp.timestamp;

	ptr = (const uint8_t *)pkt.payload;
	pend = ptr + pkt.payloadlen;

	// AV1 aggregation header
	/*
	0 1 2 3 4 5 6 7
	+-+-+-+-+-+-+-+-+
	|Z|Y| W |N|-|-|-|
	+-+-+-+-+-+-+-+-+
	*/
	z = ptr[0] & 0x80; // MUST be set to 1 if the first OBU element is an OBU fragment that is a continuation of an OBU fragment from the previous packet, and MUST be set to 0 otherwise.
	y = ptr[0] & 0x40; // MUST be set to 1 if the last OBU element is an OBU fragment that will continue in the next packet, and MUST be set to 0 otherwise.
	w = (ptr[0] & 0x30) >> 4; // two bit field that describes the number of OBU elements in the packet. This field MUST be set equal to 0 or equal to the number of OBU elements contained in the packet. If set to 0, each OBU element MUST be preceded by a length field.
	n = ptr[0] & 0x08; // MUST be set to 1 if the packet is the first packet of a coded video sequence, and MUST be set to 0 otherwise.
	(void)y;

	// if N equals 1 then Z must equal 0.
	assert(!n || 0 == z);
	if (0 == z && 1 == n) {
		// new video codec sequence
		rtp_av1_unpack_onframe(unpacker);
	}

	for (i = 1, ptr++; ptr < pend; ptr += size, i++)
	{
		if (i < w || 0 == w)
		{
			ptr = leb128(ptr, pend - ptr, &size);
		}
		else
		{
			size = pend - ptr;
		}

		// skip fragment frame OBU size
		if (ptr + size > pend)
		{
			//assert(0);
			//unpacker->size = 0;
			unpacker->lost = 1;
			//unpacker->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST;
			return -1; // invalid packet
		}

		rtp_av1_unpack_obu_append(unpacker, ptr, (int)size, (1 != i || 0 == z) ? 1 : 0);
	}

	// The RTP header Marker bit MUST be set equal to 0 
	// if the packet is not the last packet of the temporal unit, 
	// it SHOULD be set equal to 1 otherwise.
	if (pkt.rtp.m)
	{
		rtp_av1_unpack_onframe(unpacker);
	}

	return 1; // packet handled
}

struct rtp_payload_decode_t *rtp_av1_decode()
{
	static struct rtp_payload_decode_t unpacker = {
		rtp_av1_unpack_create,
		rtp_av1_unpack_destroy,
		rtp_av1_unpack_input,
	};

	return &unpacker;
}
