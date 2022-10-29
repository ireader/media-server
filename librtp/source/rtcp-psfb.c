#include "rtp-internal.h"
#include "rtp-util.h"
#include <errno.h>

static int rtcp_psfb_pli_pack(uint8_t* ptr, uint32_t bytes);
static int rtcp_psfb_sli_pack(const rtcp_sli_t* sli, int count, uint8_t* ptr, uint32_t bytes);
static int rtcp_psfb_rpsi_pack(uint8_t pt, const uint8_t* payload, uint32_t bits, uint8_t* ptr, uint32_t bytes);
static int rtcp_psfb_fir_pack(const rtcp_fir_t* fir, int count, uint8_t* ptr, uint32_t bytes);
static int rtcp_psfb_tstr_pack(const rtcp_fir_t* fir, int count, uint8_t* ptr, uint32_t bytes);
static int rtcp_psfb_tstn_pack(const rtcp_fir_t* fir, int count, uint8_t* ptr, uint32_t bytes);
static int rtcp_psfb_vbcm_pack(const rtcp_vbcm_t* vbcm, uint8_t* ptr, uint32_t bytes);
static int rtcp_psfb_pslei_pack(const uint32_t* ssrc, int count, uint8_t* ptr, uint32_t bytes);
static int rtcp_psfb_lrr_pack(const rtcp_lrr_t* lrr, int count, uint8_t* ptr, uint32_t bytes);
static int rtcp_psfb_remb_pack(const rtcp_remb_t* remb, int count, uint8_t* ptr, uint32_t bytes);

static int rtcp_psfb_pli_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_psfb_sli_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_psfb_rpsi_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_psfb_fir_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_psfb_tstr_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_psfb_tstn_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_psfb_vbcm_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_psfb_pslei_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_psfb_lrr_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_psfb_roi_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_psfb_afb_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);


// https://datatracker.ietf.org/doc/html/rfc4585#section-6.3.1
static int rtcp_psfb_pli_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	// 1. There MUST be exactly one PLI contained in the FCI field.
	// 2. PLI does not require parameters.  Therefore, the length field MUST be 2, and there MUST NOT be any Feedback Control Information.
	assert(0 == bytes);

	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header, (void)ptr;
	return 0;
}

static int rtcp_psfb_pli_pack(uint8_t* ptr, uint32_t bytes)
{
	(void)ptr, (void)bytes;
	return 0;
}

// https://datatracker.ietf.org/doc/html/rfc4585#section-6.3.2
/*
	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            First        |        Number           | PictureID |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_psfb_sli_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	size_t i;
	rtcp_sli_t* sli, sli0[32];

	if (bytes / 4 > sizeof(sli0) / sizeof(sli0[0]))
	{
		sli = calloc(bytes / 4, sizeof(*sli));
		if (!sli) return -ENOMEM;
	}
	else
	{
		sli = sli0;
		memset(sli, 0, sizeof(sli[0]) * (bytes / 4));
	}

	for (i = 0; i < bytes / 4; i++)
	{
		sli[i].first = (ptr[0] << 5) | (ptr[1] >> 3);
		sli[i].number = ((ptr[1] & 0x07) << 10) | (ptr[2] << 2) | (ptr[3] >> 6);
		sli[i].picture_id = ptr[3] & 0x3F;

		ptr += 4;
	}

	msg->u.psfb.u.sli.sli = sli;
	msg->u.psfb.u.sli.count = (int)i;
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header;
	if (sli && sli != sli0)
		free(sli);
	return 0;
}

static int rtcp_psfb_sli_pack(const rtcp_sli_t* sli, int count, uint8_t* ptr, uint32_t bytes)
{
	int i;
	for (i = 0; i < count && bytes >= 4; i++)
	{
		nbo_w32(ptr, ((sli->first & 0x1FFFF) << 19) | ((sli->number & 0x1FFFF) << 6) | (sli->picture_id & 0x3F));

		bytes -= 4;
		ptr += 4;
	}
	return i * 4;
}

// https://datatracker.ietf.org/doc/html/rfc4585#section-6.3.3
/*
	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      PB       |0| Payload Type|    Native RPSI bit string     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   defined per codec          ...                | Padding (0) |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_psfb_rpsi_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	uint8_t pb;
	uint8_t pt;

	if (bytes < 4)
		return -1;

	pb = ptr[0];
	pt = ptr[1] & 0x7F;

	msg->u.psfb.u.rpsi.pt = pt;
	msg->u.psfb.u.rpsi.len = (uint32_t)bytes * 8 - pb;
	msg->u.psfb.u.rpsi.payload = (uint8_t*)ptr + 2;
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header;
	return 0;
}

static int rtcp_psfb_rpsi_pack(uint8_t pt, const uint8_t* payload, uint32_t bits, uint8_t* ptr, uint32_t bytes)
{
	uint32_t len;
	len = (bits + 7) / 8;
	if (bytes < (2 + len + 3) / 4 * 4)
		return -1;

	ptr[0] = (uint8_t)((2 + len + 3) / 4 * 4 * 8 - (2 * 8 + bits));
	ptr[1] = pt;
	memcpy(ptr + 2, payload, len);
	return (2 + len + 3) / 4 * 4;
}

// https://www.rfc-editor.org/rfc/rfc5104.html#section-4.3.1
/*
	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                              SSRC                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Seq nr.       |    Reserved                                   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_psfb_fir_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	size_t i;
	rtcp_fir_t *fir, fir0[32];

	if (bytes / 8 > sizeof(fir0) / sizeof(fir0[0]))
	{
		fir = calloc(bytes / 8, sizeof(*fir));
		if (!fir) return -ENOMEM;
	}
	else
	{
		fir = fir0;
		memset(fir, 0, sizeof(fir[0]) * (bytes / 8));
	}

	for (i = 0; i < bytes / 8; i++)
	{
		fir[i].ssrc = nbo_r32(ptr);
		fir[i].sn = ptr[4];
		ptr += 8;
	}
	
	msg->u.psfb.u.fir.fir = fir;
	msg->u.psfb.u.fir.count = (int)i;
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header;
	if (fir && fir != fir0)
		free(fir);
	return 0;
}

static int rtcp_psfb_fir_pack(const rtcp_fir_t* fir, int count, uint8_t* ptr, uint32_t bytes)
{
	int i;
	for (i = 0; i < count && bytes >= 8; i++)
	{
		nbo_w32(ptr, fir[i].ssrc);
		nbo_w32(ptr + 4, fir[i].sn << 24);

		bytes -= 8;
		ptr += 8;
	}
	return i * 8;
}

// https://www.rfc-editor.org/rfc/rfc5104.html#section-4.3.2
/*
	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                              SSRC                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Seq nr.      |  Reserved                           | Index   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_psfb_tstr_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	size_t i;
	rtcp_fir_t* fir, fir0[32];

	if (bytes / 8 > sizeof(fir0) / sizeof(fir0[0]))
	{
		fir = calloc(bytes / 8, sizeof(*fir));
		if (!fir) return -ENOMEM;
	}
	else
	{
		fir = fir0;
		memset(fir, 0, sizeof(fir[0]) * (bytes / 8));
	}

	for (i = 0; i < bytes / 8; i++)
	{
		fir[i].ssrc = nbo_r32(ptr);
		fir[i].sn = ptr[4];
		fir[i].index = ptr[7] & 0x1F;

		ptr += 8;
	}

	msg->u.psfb.u.fir.fir = fir;
	msg->u.psfb.u.fir.count = (int)i;
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header;
	if (fir && fir != fir0)
		free(fir);
	return 0;
}

static int rtcp_psfb_tstr_pack(const rtcp_fir_t* fir, int count, uint8_t* ptr, uint32_t bytes)
{
	int i;
	for (i = 0; i < count && bytes >= 8; i++)
	{
		nbo_w32(ptr, fir[i].ssrc);
		nbo_w32(ptr + 4, (fir[i].sn << 24) | fir[i].index);

		bytes -= 8;
		ptr += 8;
	}
	return i * 8;
}

// https://www.rfc-editor.org/rfc/rfc5104.html#section-4.3.3
/*
	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                              SSRC                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Seq nr.      |  Reserved                           | Index   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_psfb_tstn_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	size_t i;
	rtcp_fir_t* fir, fir0[32];

	if (bytes / 8 > sizeof(fir0) / sizeof(fir0[0]))
	{
		fir = calloc(bytes / 8, sizeof(*fir));
		if (!fir) return -ENOMEM;
	}
	else
	{
		fir = fir0;
		memset(fir, 0, sizeof(fir[0]) * (bytes / 8));
	}

	for (i = 0; i < bytes / 8; i++)
	{
		fir[i].ssrc = nbo_r32(ptr);
		fir[i].sn = ptr[4];
		fir[i].index = ptr[7] & 0x1F;

		ptr += 8;
	}

	msg->u.psfb.u.fir.fir = fir;
	msg->u.psfb.u.fir.count = (int)i;
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header;
	if (fir && fir != fir0)
		free(fir);
	return 0;
}

static int rtcp_psfb_tstn_pack(const rtcp_fir_t* fir, int count, uint8_t* ptr, uint32_t bytes)
{
	return rtcp_psfb_tstr_pack(fir, count, ptr, bytes);
}

// https://www.rfc-editor.org/rfc/rfc5104.html#section-4.3.4
/*
   0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                              SSRC                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Seq nr.       |0| Payload Type| Length                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    VBCM Octet String....      |    Padding    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_psfb_vbcm_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	rtcp_vbcm_t vbcm;

	while(bytes > 8)
	{
		vbcm.ssrc = nbo_r32(ptr);
		vbcm.sn = ptr[4];
		vbcm.pt = ptr[5] & 0x7F;
		vbcm.len = nbo_r16(ptr + 6);
		if (vbcm.len + 8 > bytes)
			return -1;

		vbcm.payload = (uint8_t*)ptr + 8;
		bytes -= 8 + (vbcm.len + 3) / 4 * 4;
		ptr += 8 + (vbcm.len + 3) / 4 * 4;

		memcpy(&msg->u.psfb.u.vbcm, &vbcm, sizeof(vbcm));
		ctx->handler.on_rtcp(ctx->cbparam, msg);
	}

	(void)ctx, (void)header;
	return 0;
}

static int rtcp_psfb_vbcm_pack(const rtcp_vbcm_t* vbcm, uint8_t* ptr, uint32_t bytes)
{
	if (bytes < 8 + (uint32_t)(vbcm->len + 3) / 4 * 4)
		return -1;

	nbo_w32(ptr, vbcm->ssrc);
	ptr[4] = (uint8_t)vbcm->sn;
	ptr[5] = (uint8_t)vbcm->pt;
	nbo_w16(ptr + 6, (uint16_t)vbcm->len);
	memcpy(ptr + 8, vbcm->payload, vbcm->len);
	return 8 + (vbcm->len + 3) / 4 * 4;
}

// https://www.rfc-editor.org/rfc/rfc6642.html#section-5.2
/*
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                              SSRC                             |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_psfb_pslei_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	size_t i;
	uint32_t* v, v0[32];

	if (bytes / 4 > sizeof(v0) / sizeof(v0[0]))
	{
		v = calloc(bytes / 4, sizeof(*v));
		if (!v) return -ENOMEM;
	}
	else
	{
		v = v0;
		memset(v, 0, sizeof(v[0]) * (bytes / 4));
	}

	for (i = 0; i < bytes / 4; i++)
	{
		v[i] = nbo_r32(ptr);
		ptr += 4;
	}

	msg->u.psfb.u.pslei.ssrc = v;
	msg->u.psfb.u.pslei.count = (int)i;
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header;
	if (v && v != v0)
		free(v);
    return 0;
}

static int rtcp_psfb_pslei_pack(const uint32_t* ssrc, int count, uint8_t* ptr, uint32_t bytes)
{
	int i;
	for (i = 0; i < count && bytes >= 4; i++)
	{
		nbo_w32(ptr, ssrc[i]);

		bytes -= 4;
		ptr += 4;
	}
	return i * 4;
}

// https://datatracker.ietf.org/doc/html/draft-ietf-avtext-lrr-07#section-3.1
/*
	   0                   1                   2                   3
	   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	  |                              SSRC                             |
	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	  | Seq nr.       |C| Payload Type| Reserved                      |
	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	  | RES     | TTID| TLID          | RES     | CTID| CLID          |
	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_psfb_lrr_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	size_t i;
	rtcp_lrr_t* lrr, lrr0[32];

	if (bytes / 12 > sizeof(lrr0) / sizeof(lrr0[0]))
	{
		lrr = calloc(bytes / 12, sizeof(*lrr));
		if (!lrr) return -ENOMEM;
	}
	else
	{
		lrr = lrr0;
		memset(lrr, 0, sizeof(lrr[0]) * (bytes / 12));
	}

	for (i = 0; i < bytes / 12; i++)
	{
		lrr[i].ssrc = nbo_r32(ptr);
		lrr[i].sn = ptr[4];
		lrr[i].c = (ptr[5] >> 7) & 0x01;
		lrr[i].payload = ptr[5] & 0x7F;
		lrr[i].ttid = ptr[8] & 0x07;
		lrr[i].tlid = ptr[9];
		lrr[i].ctid = ptr[10] & 0x07;
		lrr[i].clid = ptr[11];
		ptr += 12;
	}

	msg->u.psfb.u.lrr.lrr = lrr;
	msg->u.psfb.u.lrr.count = (int)i;
	ctx->handler.on_rtcp(ctx->cbparam, msg);
	(void)ctx, (void)header;
	if (lrr && lrr != lrr0)
		free(lrr);
	return 0;
}

static int rtcp_psfb_lrr_pack(const rtcp_lrr_t* lrr, int count, uint8_t* ptr, uint32_t bytes)
{
	int i;
	for (i = 0; i < count && bytes >= 12; i++)
	{
		nbo_w32(ptr, lrr[i].ssrc);
		nbo_w32(ptr, (lrr[i].sn << 24) | ((lrr[i].c & 0x01) << 23) | ((lrr[i].payload & 0x7F) << 16));
		nbo_w32(ptr, ((lrr[i].ttid & 0x07) << 24) | ((lrr[i].tlid & 0xFF) << 16) | ((lrr[i].ctid & 0x07) << 8) | ((lrr[i].clid & 0xFF) << 0));

		bytes -= 12;
		ptr += 12;
	}
	return i * 12;
}

// https://portal.3gpp.org/desktopmodules/Specifications/SpecificationDetails.aspx?specificationId=1404
// 7.3.7	Video Region-of-Interest (ROI) Signaling
/*
Arbitrary ROI
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Position_X (h)| Position_X (l)| Position_Y (h)|  Position_Y(l)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Size_X (h)  |   Size_X (l)  |   Size_Y (h)  |    Size_Y(l)  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Pre-defined ROI
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           all ones                            |   ROI_ID      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   ID  | len=7 | Position_X (h)| Position_X (l)| Position_Y (h)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Position_Y (l)|   Size_X (h)  |   Size_X (l)  |   Size_Y (h)  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Size_Y (l)   |                 zero padding                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   ID  | len=0 |     ROI_ID    |      zero padding             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_psfb_roi_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	(void)ctx, (void)header, (void)msg, (void)ptr, (void)bytes;
	return 0;
}

// https://datatracker.ietf.org/doc/html/rfc4585#section-6.4
// https://datatracker.ietf.org/doc/html/draft-alvestrand-rmcat-remb-03#section-2.2
/*
	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P| FMT=15  |   PT=206      |             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  SSRC of packet sender                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  SSRC of media source                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Unique identifier 'R' 'E' 'M' 'B'                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Num SSRC     | BR Exp    |  BR Mantissa                      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   SSRC feedback                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  ...                                                          |
*/
static int rtcp_psfb_afb_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
	uint32_t i, n, exp, mantissa;
	rtcp_remb_t* remb, remb0[4];
	const uint8_t id[] = { 'R', 'E', 'M', 'B' };

	if (bytes >= 8 && 0 == memcmp(ptr, id, 4))
	{
		n = ptr[4];
		exp = (ptr[5] >> 2) & 0x3F;
		mantissa = ((ptr[5] & 0x3) << 16) | nbo_r16(ptr+6);

		ptr += 8;
		bytes -= 8;
		if (n * 4 > bytes)
			return -1;

		if (n > sizeof(remb0) / sizeof(remb0[0]))
		{
			remb = calloc(n, sizeof(*remb));
			if (!remb) return -ENOMEM;
		}
		else
		{
			remb = remb0;
			memset(remb, 0, sizeof(remb[0]) * n);
		}

		for (i = 0; i < n; i++)
		{
			remb[i].exp = exp;
			remb[i].mantissa = mantissa;
			remb[i].ssrc = nbo_r32(ptr);
			ptr += 4;
		}

		msg->u.psfb.u.afb.remb = remb;
		msg->u.psfb.u.afb.count = i;
		ctx->handler.on_rtcp(ctx->cbparam, msg);
		if (remb && remb != remb0)
			free(remb);
		return 0;
	}

	(void)ctx, (void)header;
	return 0;
}

static int rtcp_psfb_remb_pack(const rtcp_remb_t* remb, int count, uint8_t* ptr, uint32_t bytes)
{
	int i;
	const uint8_t id[] = { 'R', 'E', 'M', 'B' };

	if (count < 1 || count > 255 || (int)bytes < 4 + 4 + count * 4)
		return -E2BIG;

	memcpy(ptr, id, sizeof(id));
	nbo_w32(ptr + 4, (count << 24) | (remb[0].exp << 18) | remb[0].mantissa);

	ptr += 8;
	for (i = 0; i < count; i++)
	{
		nbo_w32(ptr, remb[i].ssrc);
		ptr += 4;
	}
	return 4 + 4 + count * 4;
}

// https://datatracker.ietf.org/doc/html/rfc4585#section-6.3
/*
*  Common Packet Format for Feedback Messages

	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|   FMT   |       PT      |          length               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  SSRC of packet sender                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  SSRC of media source                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   :            Feedback Control Information (FCI)                 :
   :                                                               :
*/
void rtcp_psfb_unpack(struct rtp_context* ctx, const rtcp_header_t* header, const uint8_t* ptr, size_t bytes)
{
	int r;
	struct rtcp_msg_t msg;
	struct rtp_member* sender;

	if (bytes < 8 /*sizeof(rtcp_fci_t)*/)
	{
		assert(0);
		return;
	}

	msg.type = RTCP_PSFB | (header->rc << 8);
	msg.ssrc = nbo_r32(ptr);
	msg.u.psfb.media = nbo_r32(ptr + 4);

	sender = rtp_sender_fetch(ctx, msg.ssrc);
	if (!sender) return; // error
	assert(sender != ctx->self);

	r = 0;
	switch (header->rc)
	{
	case RTCP_PSFB_PLI:
		r = rtcp_psfb_pli_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_PSFB_SLI:
		r = rtcp_psfb_sli_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_PSFB_RPSI:
		r = rtcp_psfb_rpsi_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_PSFB_FIR:
		r = rtcp_psfb_fir_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_PSFB_TSTR:
		r = rtcp_psfb_tstr_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_PSFB_TSTN:
		r = rtcp_psfb_tstn_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_PSFB_VBCM:
		r = rtcp_psfb_vbcm_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_PSFB_PSLEI:
		r = rtcp_psfb_pslei_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_PSFB_ROI:
		r = rtcp_psfb_roi_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_PSFB_LRR:
		r = rtcp_psfb_lrr_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	case RTCP_PSFB_AFB:
		r = rtcp_psfb_afb_unpack(ctx, header, &msg, ptr + 8, bytes - 8);
		break;

	default:
		assert(0);
		r = 0; // ignore
		break;
	}

	return;
}

int rtcp_psfb_pack(struct rtp_context* ctx, uint8_t* data, int bytes, enum rtcp_psfb_type_t id, const rtcp_psfb_t* psfb)
{
	int r;
	rtcp_header_t header;

	(void)ctx;
	if (bytes < 4 + 4 + 4)
		return 4 + 4 + 4;

	switch (id)
	{
	case RTCP_PSFB_PLI:
		r = rtcp_psfb_pli_pack(data + 12, bytes - 12);
		break;

	case RTCP_PSFB_SLI:
		r = rtcp_psfb_sli_pack(psfb->u.sli.sli, psfb->u.sli.count, data + 12, bytes - 12);
		break;

	case RTCP_PSFB_RPSI:
		r = rtcp_psfb_rpsi_pack(psfb->u.rpsi.pt, psfb->u.rpsi.payload, psfb->u.rpsi.len, data + 12, bytes - 12);
		break;

	case RTCP_PSFB_FIR:
		r = rtcp_psfb_fir_pack(psfb->u.fir.fir, psfb->u.fir.count, data + 12, bytes - 12);
		break;

	case RTCP_PSFB_TSTR:
		r = rtcp_psfb_tstr_pack(psfb->u.fir.fir, psfb->u.fir.count, data + 12, bytes - 12);
		break;

	case RTCP_PSFB_TSTN:
		r = rtcp_psfb_tstn_pack(psfb->u.fir.fir, psfb->u.fir.count, data + 12, bytes - 12);
		break;

	case RTCP_PSFB_VBCM:
		r = rtcp_psfb_vbcm_pack(&psfb->u.vbcm, data + 12, bytes - 12);
		break;

	case RTCP_PSFB_PSLEI:
		r = rtcp_psfb_pslei_pack(psfb->u.pslei.ssrc, psfb->u.pslei.count, data + 12, bytes - 12);
		break;

	case RTCP_PSFB_LRR:
		r = rtcp_psfb_lrr_pack(psfb->u.lrr.lrr, psfb->u.lrr.count, data + 12, bytes - 12);
		break;

	case RTCP_PSFB_REMB:
		r = rtcp_psfb_remb_pack(psfb->u.afb.remb, psfb->u.afb.count, data + 12, bytes - 12);
		break;

	case RTCP_PSFB_ROI:
	default:
		assert(0);
		return -1;
	}

	header.v = 2;
	header.p = 0;
	header.pt = RTCP_PSFB;
	header.rc = id;
	header.length = (r + 8 + 3) / 4;
	nbo_write_rtcp_header(data, &header);

	nbo_w32(data + 4, ctx->self->ssrc);
	//nbo_w32(data + 4, psfb->sender);
	nbo_w32(data + 8, psfb->media);

	//assert(8 == (header.length + 1) * 4);
	return header.length * 4 + 4;
}

#if defined(_DEBUG) || defined(DEBUG)
static void rtcp_on_psfb_test(void* param, const struct rtcp_msg_t* msg)
{
	int r;
	static uint8_t buffer[1400];
	switch (msg->type & 0xFF)
	{
	case RTCP_PSFB:
		switch ((msg->type >> 8) & 0xFF)
		{
		case RTCP_PSFB_PLI:
			r = rtcp_psfb_pli_pack(buffer, sizeof(buffer));
			assert(0 == r);
			break;

		case RTCP_PSFB_FIR:
			assert(1 == msg->u.psfb.u.fir.count);
			assert(0x23456789 == msg->u.psfb.u.fir.fir[0].ssrc && 13 == msg->u.psfb.u.fir.fir[0].sn);
			r = rtcp_psfb_fir_pack(msg->u.psfb.u.fir.fir, msg->u.psfb.u.fir.count, buffer, sizeof(buffer));
			assert(r == 8 && 0 == memcmp(buffer, param, r));
			break;

		case RTCP_PSFB_REMB:
			assert(3 == msg->u.psfb.u.afb.count);
			assert(0x23456789 == msg->u.psfb.u.afb.remb[0].ssrc && 1 == msg->u.psfb.u.afb.remb[0].exp && 0x3fb93 == msg->u.psfb.u.afb.remb[0].mantissa);
			assert(0x2345678a == msg->u.psfb.u.afb.remb[1].ssrc && 1 == msg->u.psfb.u.afb.remb[1].exp && 0x3fb93 == msg->u.psfb.u.afb.remb[1].mantissa);
			assert(0x2345678b == msg->u.psfb.u.afb.remb[2].ssrc && 1 == msg->u.psfb.u.afb.remb[2].exp && 0x3fb93 == msg->u.psfb.u.afb.remb[2].mantissa);
			r = rtcp_psfb_remb_pack(msg->u.psfb.u.afb.remb, msg->u.psfb.u.afb.count, buffer, sizeof(buffer));
			assert(r == 20 && 0 == memcmp(buffer, param, r));
			break;

		default:
			break;
		}
		break;

	default:
		assert(0);
	}
}

static void rtcp_rtpfb_pli_test(void)
{
	struct rtcp_msg_t msg;
	struct rtp_context rtp;
	rtp.handler.on_rtcp = rtcp_on_psfb_test;
	rtp.cbparam = NULL;

	msg.type = (RTCP_PSFB_PLI << 8) | RTCP_PSFB;
	assert(0 == rtcp_psfb_pli_unpack(&rtp, NULL, &msg, NULL, 0));
}

static void rtcp_rtpfb_fir_test(void)
{
	const uint8_t data[] = { 0x23, 0x45, 0x67, 0x89, 0x0d, 0x00, 0x00, 0x00 };

	struct rtcp_msg_t msg;
	struct rtp_context rtp;
	rtp.handler.on_rtcp = rtcp_on_psfb_test;
	rtp.cbparam = (void*)data;

	msg.type = (RTCP_PSFB_FIR << 8) | RTCP_PSFB;
	assert(0 == rtcp_psfb_fir_unpack(&rtp, NULL, &msg, data, sizeof(data)));
}

static void rtcp_rtpfb_remb_test(void)
{
	const uint8_t data[] = { 'R', 'E', 'M', 'B', 0x03, 0x07, 0xfb, 0x93, 0x23, 0x45, 0x67, 0x89, 0x23, 0x45, 0x67, 0x8a, 0x23, 0x45, 0x67, 0x8b };

	struct rtcp_msg_t msg;
	struct rtp_context rtp;
	rtp.handler.on_rtcp = rtcp_on_psfb_test;
	rtp.cbparam = (void*)data;

	msg.type = (RTCP_PSFB_REMB << 8) | RTCP_PSFB;
	assert(0 == rtcp_psfb_afb_unpack(&rtp, NULL, &msg, data, sizeof(data)));
}

void rtcp_psfb_test(void)
{
	rtcp_rtpfb_pli_test();
	rtcp_rtpfb_fir_test();
	rtcp_rtpfb_remb_test();
}
#endif
