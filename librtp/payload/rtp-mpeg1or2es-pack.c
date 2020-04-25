/// RFC2250 3. Encapsulation of MPEG Elementary Streams (p4)
/// 3.1 MPEG Video elementary streams
/// 1. The MPEG Video_Sequence_Header, when present, will always be at the beginning of an RTP payload
/// 2. An MPEG GOP_header, when present, will always be at the beginning of the RTP payload, or will follow a Video_Sequence_Header
/// 3. An MPEG Picture_Header, when present, will always be at the beginning of a RTP payload, or will follow a GOP_header
/// 4. An implementation based on this encapsulation assumes that the Video_Sequence_Header is repeated periodically in the MPEG bitstream.
/// 5. The beginning of a slice must either be the first data in a packet(after any MPEG ES headers) or must follow after some integral number of slices in a packet.
///
/// minimum RTP payload size of 261 bytes must be supported to contain the largest single header
///
/// 3.2 MPEG Audio elementary streams
/// 1. Multiple audio frames may be encapsulated within one RTP packet.
///
/// 3.3 RTP Fixed Header for MPEG ES encapsulation (p7)
/// 1. M bit: For video, set to 1 on packet containing MPEG frame end code, 0 otherwise.
///           For audio, set to 1 on first packet of a "talk-spurt," 0 otherwise.
/// 2. timestamp: 32 bit 90K Hz timestamp representing the target transmission time for the first byte of the packet

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// ISO/IEC 13818-2: 1995 (E) Table 6-1 ¡ª Start code values (p41)
#define MPEG2VIDEO_PICTURE	0x00
#define MPEG2VIDEO_SLICE	0x01 // ~0xAF
#define MPEG2VIDEO_SEQUENCE	0xB3
#define MPEG2VIDEO_GROUP	0xB8

#define N_MPEG12_HEADER 4

#define KHz 90 // 90000Hz

struct rtp_encode_mpeg2es_t
{
	struct rtp_packet_t pkt;
	struct rtp_payload_t handler;
	void* cbparam;
	int size;
};

struct mpeg2_video_header_t
{
	unsigned int begin_of_sequence : 1;
	//unsigned int begin_of_slice : 1;
	//unsigned int end_of_slice : 1;
	unsigned int frame_type : 3; // This value is constant for each RTP packet of a given picture.
	unsigned int temporal_reference : 10;// This value is constant for all RTP packets of a given picture.

	// Obtained from the most recent picture header, and are
	// constant for each RTP packet of a given picture.
	unsigned int FBV : 1;
	unsigned int BFC : 3;
	unsigned int FFV : 1;
	unsigned int FFC : 3;
};

static void* rtp_mpeg2es_pack_create(int size, uint8_t pt, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam)
{
	struct rtp_encode_mpeg2es_t *packer;
	packer = (struct rtp_encode_mpeg2es_t *)calloc(1, sizeof(*packer));
	if (!packer) return NULL;

	assert(RTP_PAYLOAD_MP3 == pt || RTP_PAYLOAD_MPV == pt);
	memcpy(&packer->handler, handler, sizeof(packer->handler));
	packer->cbparam = cbparam;
	packer->size = size;

	packer->pkt.rtp.v = RTP_VERSION;
	packer->pkt.rtp.pt = pt;
	packer->pkt.rtp.seq = seq;
	packer->pkt.rtp.ssrc = ssrc;
	packer->pkt.rtp.m = (RTP_PAYLOAD_MP3 == pt) ? 1 : 0; // set to 1 on first packet of a "talk-spurt," 0 otherwise.
	return packer;
}

static void rtp_mpeg2es_pack_destroy(void* pack)
{
	struct rtp_encode_mpeg2es_t *packer;
	packer = (struct rtp_encode_mpeg2es_t *)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static void rtp_mpeg2es_pack_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_encode_mpeg2es_t *packer;
	packer = (struct rtp_encode_mpeg2es_t *)pack;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

// 3.5 MPEG Audio-specific header
/*
 0               1               2               3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|             MBZ               |            Frag_offset        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtp_mpeg2es_pack_audio(struct rtp_encode_mpeg2es_t *packer, const uint8_t* audio, int bytes)
{
	int n;
	int offset;
	uint8_t *rtp;

	for (offset = 0; bytes > 0; ++packer->pkt.rtp.seq)
	{
		packer->pkt.payload = audio;
		packer->pkt.payloadlen = (bytes + N_MPEG12_HEADER + RTP_FIXED_HEADER) <= packer->size ? bytes : (packer->size - N_MPEG12_HEADER - RTP_FIXED_HEADER);
		audio += packer->pkt.payloadlen;
		bytes -= packer->pkt.payloadlen;

		n = RTP_FIXED_HEADER + N_MPEG12_HEADER + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
		if (!rtp) return -ENOMEM;

		n = rtp_packet_serialize_header(&packer->pkt, rtp, n);
		if (n != RTP_FIXED_HEADER)
		{
			assert(0);
			return -1;
		}
		packer->pkt.rtp.m = 0; // set to 1 on first packet of a "talk-spurt," 0 otherwise.

		/* build fragmented packet */
		rtp[n + 0] = 0;
		rtp[n + 1] = 0;
		rtp[n + 2] = (uint8_t)(offset >> 8);
		rtp[n + 3] = (uint8_t)offset;
		memcpy(rtp + n + N_MPEG12_HEADER, packer->pkt.payload, packer->pkt.payloadlen);
		offset += packer->pkt.payloadlen;

		packer->handler.packet(packer->cbparam, rtp, n + N_MPEG12_HEADER + packer->pkt.payloadlen, packer->pkt.rtp.timestamp, 0);
		packer->handler.free(packer->cbparam, rtp);
	}

	return 0;
}

static const uint8_t* mpeg2_start_code_prefix_find(const uint8_t* p, const uint8_t* end)
{
	int i;
	for (i = 0; p + i + 4 < end; i++)
	{
		if (0x00 == p[i] && 0x00 == p[i + 1] && 0x01 == p[i + 2])
			return p + i;
	}
	return end;
}

// 3.4 MPEG Video-specific header
/*
 0               1               2               3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   MBZ   |T|        TR         | |N|S|B|E|  P  | | BFC | | FFC |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                AN              FBV     FFV
*/

// 3.4.1 MPEG-2 Video-specific header extension
/*
0               1               2               3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|X|E|f_[0,0]|f_[0,1]|f_[1,0]|f_[1,1]| DC| PS|T|P|C|Q|V|A|R|H|G|D|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtp_mpeg2es_pack_slice(struct rtp_encode_mpeg2es_t *packer, const uint8_t* video, int bytes, struct mpeg2_video_header_t* h, int marker)
{
	int n;
	uint8_t *rtp;
	uint8_t begin_of_slice;
	uint8_t end_of_slice;
	uint8_t begin_of_sequence;

	for (begin_of_slice = 1; bytes > 0; ++packer->pkt.rtp.seq)
	{
		packer->pkt.payload = video;
		packer->pkt.payloadlen = (bytes + N_MPEG12_HEADER + RTP_FIXED_HEADER) <= packer->size ? bytes : (packer->size - N_MPEG12_HEADER - RTP_FIXED_HEADER);
		video += packer->pkt.payloadlen;
		bytes -= packer->pkt.payloadlen;

		n = RTP_FIXED_HEADER + N_MPEG12_HEADER + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
		if (!rtp) return -ENOMEM;

		packer->pkt.rtp.m = (marker && 0==bytes) ? 1 : 0; // set to 1 on packet containing MPEG frame end code
		n = rtp_packet_serialize_header(&packer->pkt, rtp, n);
		if (n != RTP_FIXED_HEADER)
		{
			assert(0);
			return -1;
		}

		/* build fragmented packet */
		end_of_slice = bytes ? 0 : 1;
		begin_of_sequence = (h->begin_of_sequence && begin_of_slice) ? 1 : 0;
		rtp[n + 0] = (uint8_t)(h->temporal_reference >> 8) & 0x03;
		rtp[n + 1] = (uint8_t)h->temporal_reference;
		rtp[n + 2] = (uint8_t)((begin_of_sequence << 5) | (begin_of_slice << 4) | (end_of_slice << 3) | h->frame_type);
		rtp[n + 3] = (uint8_t)((h->FBV << 7) | (h->BFC << 4) | (h->FFV << 3) | h->FFC);
		memcpy(rtp + n + N_MPEG12_HEADER, packer->pkt.payload, packer->pkt.payloadlen);
		begin_of_slice = 0;

		packer->handler.packet(packer->cbparam, rtp, n + N_MPEG12_HEADER + packer->pkt.payloadlen, packer->pkt.rtp.timestamp, 0);
		packer->handler.free(packer->cbparam, rtp);
	}

	return 0;
}

static int mpeg2_video_header_parse(struct mpeg2_video_header_t* mpeg2vh, const uint8_t* data, int bytes)
{
	if (bytes < 4)
		return -1;

	if (MPEG2VIDEO_PICTURE == data[3])
	{
		if (bytes < 9)
			return -1;

		// ISO/IEC 13818-2: 1995 (E) 6.2.3 Picture header (p47)
		/*
		picture_header() {
			picture_start_code 32 bslbf
			temporal_reference 10 uimsbf
			picture_coding_type 3 uimsbf
			vbv_delay 16 uimsbf
			if ( picture_coding_type == 2 || picture_coding_type == 3) {
				full_pel_forward_vector 1 bslbf
				forward_f_code 3 bslbf
			}
			if ( picture_coding_type == 3 ) {
				full_pel_backward_vector 1 bslbf
				backward_f_code 3 bslbf
			}
		}
		*/
		mpeg2vh->frame_type = (data[5] >> 3) & 0x07;
		mpeg2vh->temporal_reference = (data[4] << 2) | (data[5] >> 6);

		if (2 == mpeg2vh->frame_type)
		{
			mpeg2vh->FFV = (uint8_t)(data[7] >> 2) & 0x01;
			mpeg2vh->FFC = (uint8_t)((data[7] & 0x03) << 1) | ((data[8] >> 7) & 0x01);
		}
		else if (3 == mpeg2vh->frame_type)
		{
			mpeg2vh->FFV = (uint8_t)(data[7] >> 2) & 0x01;
			mpeg2vh->FFC = (uint8_t)((data[7] & 0x03) << 1) | ((data[8] >> 7) & 0x01);
			mpeg2vh->FBV = (uint8_t)(data[8] >> 6) & 0x01;
			mpeg2vh->BFC = (uint8_t)(data[8] >> 3) & 0x07;
		}
	}
	else if (MPEG2VIDEO_SEQUENCE == data[3] || MPEG2VIDEO_GROUP == data[3])
	{
		mpeg2vh->begin_of_sequence = 1;
	}

	return 0;
}

static int rtp_mpeg2es_pack_video(struct rtp_encode_mpeg2es_t *packer, const uint8_t* video, int bytes)
{
	int r;
	const uint8_t *p, *pnext, *pend;
	struct mpeg2_video_header_t mpeg2vh;
	memset(&mpeg2vh, 0, sizeof(mpeg2vh));

	pend = video + bytes;
	p = mpeg2_start_code_prefix_find(video, pend);
	for (r = 0; p < pend && 0 == r; p = pnext)
	{
		size_t nalu_size;

		mpeg2vh.begin_of_sequence = 0;
		mpeg2_video_header_parse(&mpeg2vh, p, (int)(pend - p));
		
		if (pend - p + N_MPEG12_HEADER + RTP_FIXED_HEADER <= packer->size)
		{
			nalu_size = pend - p;
			pnext = pend;
		}
		else
		{
			// current frame end position
			pnext = mpeg2_start_code_prefix_find(p + 4, pend);

			// try to put multi-slice into together
			while(pnext - p + N_MPEG12_HEADER + RTP_FIXED_HEADER < packer->size)
			{
				const uint8_t* pnextnext;
				pnextnext = mpeg2_start_code_prefix_find(pnext + 4, pend);
				if (pnextnext - p + N_MPEG12_HEADER + RTP_FIXED_HEADER > packer->size)
					break;

				// merge and get information
				mpeg2_video_header_parse(&mpeg2vh, pnext, (int)(pend - pnext));
				pnext = pnextnext;
			}
		}

		r = rtp_mpeg2es_pack_slice(packer, p, (int)(pnext - p), &mpeg2vh, (pnext == pend) ? 1 : 0);
	}

	return r;
}

static int rtp_mpeg2es_pack_input(void* pack, const void* data, int bytes, uint32_t timestamp)
{
	struct rtp_encode_mpeg2es_t *packer;
	packer = (struct rtp_encode_mpeg2es_t*)pack;
	assert(packer->pkt.rtp.timestamp != timestamp || !packer->pkt.payload /*first packet*/);
	packer->pkt.rtp.timestamp = timestamp; //(uint32_t)(time * KHz); // ms -> 90KHZ (RFC2250 p7)
	return RTP_PAYLOAD_MP3 == packer->pkt.rtp.pt ?
		rtp_mpeg2es_pack_audio(packer, (const uint8_t*)data, bytes) :
		rtp_mpeg2es_pack_video(packer, (const uint8_t*)data, bytes);
}

// MPV/MPA (MPEG-1/MPEG-2 Audio/Video Elementary Stream)
struct rtp_payload_encode_t *rtp_mpeg1or2es_encode()
{
	static struct rtp_payload_encode_t encode = {
		rtp_mpeg2es_pack_create,
		rtp_mpeg2es_pack_destroy,
		rtp_mpeg2es_pack_get_info,
		rtp_mpeg2es_pack_input,
	};

	return &encode;
}
