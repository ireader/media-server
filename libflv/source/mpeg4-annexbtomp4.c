// ISO/IEC 14496-1:2010(E)
// Annex I: Usage of ITU-T Recommendation H.264 | ISO/IEC 14496-10 AVC (p150)
//
// 1. Start Codes shall not be present in the stream. The field indicating the size of each following NAL unit
//    shall be added before NAL unit.The size of this field is defined in DecoderSpecificInfo.
// 2. It is recommended encapsulating one NAL unit in one SL packet when it is delivered over lossy environment.

#include "mpeg4-avc.h"
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#define H264_NAL_IDR		5 // Coded slice of an IDR picture
#define H264_NAL_SPS		7 // Sequence parameter set
#define H264_NAL_PPS		8 // Picture parameter set
#define H264_NAL_AUD		9 // Access unit delimiter

struct h264_annexbtomp4_handle_t
{
	struct mpeg4_avc_t* avc;
	uint8_t* avcptr;
	int errcode;
	int* update; // avc sps/pps update flags
	int* vcl;

	uint8_t* out;
	int bytes;
	int capacity;
};

static const uint8_t* h264_startcode(const uint8_t *data, int bytes)
{
	int i;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == data[i] && 0x00 == data[i - 1] && 0x00 == data[i - 2])
			return data + i + 1;
	}

	return NULL;
}

///@param[in] h264 H.264 byte stream format data(A set of NAL units)
void mpeg4_h264_annexb_nalu(const void* h264, int bytes, void (*handler)(void* param, const uint8_t* nalu, int bytes), void* param)
{
	ptrdiff_t n;
	const uint8_t* p, *next, *end;

	end = (const uint8_t*)h264 + bytes;
	p = h264_startcode((const uint8_t*)h264, bytes);

	while (p)
	{
		next = h264_startcode(p, (int)(end - p));
		if (next)
		{
			n = next - p - 3;
		}
		else
		{
			n = end - p;
		}

		while (n > 0 && 0 == p[n - 1]) n--; // filter tailing zero

		assert(n > 0);
		if (n > 0)
		{
			handler(param, p, (int)n);
		}

		p = next;
	}
}

uint8_t mpeg4_h264_read_ue(const uint8_t* data, int bytes, int* offset)
{
	int bit, i;
	int leadingZeroBits = -1;

	for (bit = 0; !bit && *offset / 8 < bytes; ++leadingZeroBits)
	{
		bit = (data[*offset / 8] >> (7 - (*offset % 8))) & 0x01;
		++*offset;
	}

	bit = 0;
	assert(leadingZeroBits < 32);
	for (i = 0; i < leadingZeroBits && *offset / 8 < bytes; i++)
	{
		bit = (bit << 1) | ((data[*offset / 8] >> (7 - (*offset % 8))) & 0x01);
		++*offset;
	}

	return (uint8_t)((1 << leadingZeroBits) - 1 + bit);
}

static void mpeg4_avc_remove(struct mpeg4_avc_t* avc, uint8_t* ptr, int bytes, const uint8_t* end)
{
	uint8_t i;
	assert(ptr >= avc->data && ptr + bytes <= end && end <= avc->data + sizeof(avc->data));
	memmove(ptr, ptr + bytes, end - ptr - bytes);

	for (i = 0; i < avc->nb_sps; i++)
	{
		if (avc->sps[i].data > ptr)
			avc->sps[i].data -= bytes;
	}

	for (i = 0; i < avc->nb_pps; i++)
	{
		if (avc->pps[i].data > ptr)
			avc->pps[i].data -= bytes;
	}
}

static int h264_sps_copy(struct h264_annexbtomp4_handle_t* mp4, const uint8_t* nalu, int bytes)
{
	int i;
	int offset;
    uint8_t spsid;

	if (bytes < 4 + 1)
	{
		assert(0);
		mp4->errcode = -1;
		return -1; // invalid length
	}

	offset = 4 * 8; // 1-NALU + 3-profile+flags+level
	spsid = mpeg4_h264_read_ue(nalu, bytes, &offset);

	for (i = 0; i < mp4->avc->nb_sps; i++)
	{
		offset = 4 * 8; // reset offset
		if (spsid == mpeg4_h264_read_ue(mp4->avc->sps[i].data, mp4->avc->sps[i].bytes, &offset))
		{
			if (bytes == mp4->avc->sps[i].bytes && 0 == memcmp(nalu, mp4->avc->sps[i].data, bytes))
				return 0; // do nothing

			if (bytes > mp4->avc->sps[i].bytes && mp4->avcptr + (bytes - mp4->avc->sps[i].bytes) > mp4->avc->data + sizeof(mp4->avc->data))
			{
				assert(0);
				mp4->errcode = -1;
				return -1; // too big
			}

			mpeg4_avc_remove(mp4->avc, mp4->avc->sps[i].data, mp4->avc->sps[i].bytes, mp4->avcptr);
			mp4->avcptr -= mp4->avc->sps[i].bytes;

			if (mp4->update) *mp4->update = 1; // set update flag
			mp4->avc->sps[i].data = mp4->avcptr;
			mp4->avc->sps[i].bytes = (uint16_t)bytes;
			memcpy(mp4->avcptr, nalu, bytes);
			mp4->avcptr += bytes;
			return 0;
		}
	}

	// copy new
	assert(mp4->avc->nb_sps < sizeof(mp4->avc->sps) / sizeof(mp4->avc->sps[0]));
	if (mp4->avc->nb_sps >= sizeof(mp4->avc->sps) / sizeof(mp4->avc->sps[0])
		|| mp4->avcptr + bytes > mp4->avc->data + sizeof(mp4->avc->data))
	{
		assert(0);
		mp4->errcode = -1;
		return -1;
	}

	if (mp4->update) *mp4->update = 1; // set update flag
	mp4->avc->sps[mp4->avc->nb_sps].data = mp4->avcptr;
	mp4->avc->sps[mp4->avc->nb_sps].bytes = (uint16_t)bytes;
	memcpy(mp4->avcptr, nalu, bytes);
	mp4->avcptr += bytes;
	++mp4->avc->nb_sps;
	return 0;
}

static int h264_pps_copy(struct h264_annexbtomp4_handle_t* mp4, const uint8_t* nalu, int bytes)
{
	int i;
    int offset;
	uint8_t spsid;
	uint8_t ppsid;

	if (bytes < 1 + 1)
	{
		assert(0);
		mp4->errcode = -1;
		return -1; // invalid length
	}

	offset = 1 * 8; // 1-NALU
	spsid = mpeg4_h264_read_ue(nalu, bytes, &offset);
	ppsid = mpeg4_h264_read_ue(nalu, bytes, &offset);

	for (i = 0; i < mp4->avc->nb_pps; i++)
	{
		offset = 1 * 8; // reset offset
		if (spsid == mpeg4_h264_read_ue(mp4->avc->pps[i].data, mp4->avc->pps[i].bytes, &offset) && ppsid == mpeg4_h264_read_ue(mp4->avc->pps[i].data, mp4->avc->pps[i].bytes, &offset))
		{
			if (bytes == mp4->avc->pps[i].bytes && 0 == memcmp(nalu, mp4->avc->pps[i].data, bytes))
				return 0; // do nothing

			if (bytes > mp4->avc->pps[i].bytes && mp4->avcptr + (bytes - mp4->avc->pps[i].bytes) > mp4->avc->data + sizeof(mp4->avc->data))
			{
				assert(0);
				mp4->errcode = -1;
				return -1; // too big
			}

			mpeg4_avc_remove(mp4->avc, mp4->avc->pps[i].data, mp4->avc->pps[i].bytes, mp4->avcptr);
			mp4->avcptr -= mp4->avc->pps[i].bytes;

			if (mp4->update) *mp4->update = 1; // set update flag
			mp4->avc->pps[i].data = mp4->avcptr;
			mp4->avc->pps[i].bytes = (uint16_t)bytes;
			memcpy(mp4->avcptr, nalu, bytes);
			mp4->avcptr += bytes;
			return 0;
		}
	}

	// copy new
	assert((unsigned int)mp4->avc->nb_pps < sizeof(mp4->avc->pps) / sizeof(mp4->avc->pps[0]));
	if ((unsigned int)mp4->avc->nb_pps >= sizeof(mp4->avc->pps) / sizeof(mp4->avc->pps[0])
		|| mp4->avcptr + bytes > mp4->avc->data + sizeof(mp4->avc->data))
	{
		assert(0);
		mp4->errcode = -1;
		return -1;
	}

	if(mp4->update) *mp4->update = 1; // set update flag
	mp4->avc->pps[mp4->avc->nb_pps].data = mp4->avcptr;
	mp4->avc->pps[mp4->avc->nb_pps].bytes = (uint16_t)bytes;
	memcpy(mp4->avcptr, nalu, bytes);
	mp4->avcptr += bytes;
	++mp4->avc->nb_pps;
	return 0;
}

static void h264_handler(void* param, const uint8_t* nalu, int bytes)
{
	int nalutype;	
	struct h264_annexbtomp4_handle_t* mp4;
	mp4 = (struct h264_annexbtomp4_handle_t*)param;

	if (bytes < 1)
	{
		assert(0);
		return;
	}

	nalutype = (nalu[0]) & 0x1f;
	switch (nalutype)
	{
	case H264_NAL_SPS:
		if (0 == h264_sps_copy(mp4, nalu, bytes) && 1 == mp4->avc->nb_sps)
		{
			// update profile/level once only
			mp4->avc->profile = nalu[1];
			mp4->avc->compatibility = nalu[2];
			mp4->avc->level = nalu[3];
		}
        break;

	case H264_NAL_PPS:
		h264_pps_copy(mp4, nalu, bytes);
        break;

#if defined(H2645_FILTER_AUD)
	case H264_NAL_AUD:
		return; // ignore AUD
#endif
	}

	// IDR-1, B/P-2, other-0
	if (mp4->vcl && 1 <= nalutype && nalutype <= H264_NAL_IDR)
		*mp4->vcl = nalutype == H264_NAL_IDR ? 1 : 2;

	if (mp4->capacity >= mp4->bytes + bytes + 4)
	{
		mp4->out[mp4->bytes + 0] = (uint8_t)((bytes >> 24) & 0xFF);
		mp4->out[mp4->bytes + 1] = (uint8_t)((bytes >> 16) & 0xFF);
		mp4->out[mp4->bytes + 2] = (uint8_t)((bytes >> 8) & 0xFF);
		mp4->out[mp4->bytes + 3] = (uint8_t)((bytes >> 0) & 0xFF);
		memcpy(mp4->out + mp4->bytes + 4, nalu, bytes);
		mp4->bytes += bytes + 4;
	}
	else
	{
		mp4->errcode = -1;
	}
}

int h264_annexbtomp4(struct mpeg4_avc_t* avc, const void* data, int bytes, void* out, int size, int* vcl, int* update)
{
	uint8_t i;
	struct h264_annexbtomp4_handle_t h;
	memset(&h, 0, sizeof(h));
	h.avcptr = avc->data;
	h.avc = avc;
	h.vcl = vcl;
	h.update = update;
	h.out = (uint8_t*)out;
	h.capacity = size;
	if (vcl) *vcl = 0;
	if (update) *update = 0;

	for (i = 0; i < avc->nb_sps; i++)
	{
		if (avc->sps[i].data >= h.avcptr)
			h.avcptr = avc->sps[i].data + avc->sps[i].bytes;
	}

	for (i = 0; i < avc->nb_pps; i++)
	{
		if (avc->pps[i].data >= h.avcptr)
			h.avcptr = avc->pps[i].data + avc->pps[i].bytes;
	}
	
	mpeg4_h264_annexb_nalu(data, bytes, h264_handler, &h);
	avc->nalu = 4;
	return 0 == h.errcode ? h.bytes : 0;
}

/// h264_is_new_access_unit H.264 new access unit(frame)
/// @return 1-new access, 0-not a new access
int h264_is_new_access_unit(const uint8_t* nalu, size_t bytes)
{
    enum { NAL_NIDR = 1, NAL_PARTITION_A = 2, NAL_IDR = 5, NAL_SEI = 6, NAL_SPS = 7, NAL_PPS = 8, NAL_AUD = 9, };
    
    uint8_t nal_type;
    
    if(bytes < 2)
        return 0;
    
    nal_type = nalu[0] & 0x1f;
    
    // 7.4.1.2.3 Order of NAL units and coded pictures and association to access units
    if(NAL_AUD == nal_type || NAL_SPS == nal_type || NAL_PPS == nal_type || NAL_SEI == nal_type || (14 <= nal_type && nal_type <= 18))
        return 1;
    
    // 7.4.1.2.4 Detection of the first VCL NAL unit of a primary coded picture
    if(NAL_NIDR == nal_type || NAL_PARTITION_A == nal_type || NAL_IDR == nal_type)
    {
        // Live555 H264or5VideoStreamParser::parse
        // The high-order bit of the byte after the "nal_unit_header" tells us whether it's
        // the start of a new 'access unit' (and thus the current NAL unit ends an 'access unit'):
        return (nalu[1] & 0x80) ? 1 : 0; // first_mb_in_slice
    }
    
    return 0;
}

#if defined(_DEBUG) || defined(DEBUG)
static void mpeg4_annexbtomp4_test2(void)
{
	const uint8_t sps[] = { 0x00,0x00,0x00,0x01,0x67,0x42,0xe0,0x1e,0xab,0xcd, };
	const uint8_t pps[] = { 0x00,0x00,0x00,0x01,0x28,0xce,0x3c,0x80 };
	const uint8_t sps1[] = { 0x00,0x00,0x00,0x01,0x67,0x42,0xe0,0x1e,0x4b,0xcd, 0x01 };
	const uint8_t pps1[] = { 0x00,0x00,0x00,0x01,0x28,0xce,0x3c,0x80, 0x01 };
	const uint8_t sps2[] = { 0x00,0x00,0x00,0x01,0x67,0x42,0xe0,0x1e,0xab };
	const uint8_t pps2[] = { 0x00,0x00,0x00,0x01,0x28,0xce,0x3c };

	int vcl, update;
	uint8_t buffer[128];
	struct mpeg4_avc_t avc;
	memset(&avc, 0, sizeof(avc));

	h264_annexbtomp4(&avc, sps, sizeof(sps), buffer, sizeof(buffer), &vcl, &update);
	assert(0 == vcl && 1 == update);
	h264_annexbtomp4(&avc, pps, sizeof(pps), buffer, sizeof(buffer), &vcl, &update);
	assert(0 == vcl && 1 == update && 1 == avc.nb_sps && avc.sps[0].bytes == sizeof(sps)-4 && 0 == memcmp(avc.sps[0].data, sps+4, sizeof(sps) - 4) && 1 == avc.nb_pps && avc.pps[0].bytes == sizeof(pps) - 4 && 0 == memcmp(avc.pps[0].data, pps+4, sizeof(pps) - 4));

	h264_annexbtomp4(&avc, sps1, sizeof(sps1), buffer, sizeof(buffer), &vcl, &update);
	assert(0 == vcl && 1 == update && 2 == avc.nb_sps && avc.sps[0].bytes == sizeof(sps) - 4 && avc.sps[1].bytes == sizeof(sps1) - 4 && 0 == memcmp(avc.sps[0].data, sps+4, sizeof(sps) - 4) && 0 == memcmp(avc.sps[1].data, sps1 + 4, sizeof(sps1) - 4) && 1 == avc.nb_pps && avc.pps[0].bytes == sizeof(pps) - 4 && 0 == memcmp(avc.pps[0].data, pps + 4, sizeof(pps) - 4));
	
	h264_annexbtomp4(&avc, pps1, sizeof(pps1), buffer, sizeof(buffer), &vcl, &update);
	assert(0 == vcl && 1 == update && 2 == avc.nb_sps && avc.sps[0].bytes == sizeof(sps) - 4 && avc.sps[1].bytes == sizeof(sps1) - 4 && 0 == memcmp(avc.sps[0].data, sps + 4, sizeof(sps) - 4) && 0 == memcmp(avc.sps[1].data, sps1 + 4, sizeof(sps1) - 4) && 1 == avc.nb_pps && avc.pps[0].bytes == sizeof(pps1) - 4 && 0 == memcmp(avc.pps[0].data, pps1 + 4, sizeof(pps1) - 4));
	
	h264_annexbtomp4(&avc, sps2, sizeof(sps2), buffer, sizeof(buffer), &vcl, &update);
	assert(0 == vcl && 1 == update && 2 == avc.nb_sps && avc.sps[0].bytes == sizeof(sps2) - 4 && avc.sps[1].bytes == sizeof(sps1) - 4 && 0 == memcmp(avc.sps[0].data, sps2 + 4, sizeof(sps2) - 4) && 0 == memcmp(avc.sps[1].data, sps1 + 4, sizeof(sps1) - 4) && 1 == avc.nb_pps && avc.pps[0].bytes == sizeof(pps1) - 4 && 0 == memcmp(avc.pps[0].data, pps1 + 4, sizeof(pps1) - 4));
	
	h264_annexbtomp4(&avc, pps2, sizeof(pps2), buffer, sizeof(buffer), &vcl, &update);
	assert(0 == vcl && 1 == update && 2 == avc.nb_sps && avc.sps[0].bytes == sizeof(sps2) - 4 && avc.sps[1].bytes == sizeof(sps1) - 4 && 0 == memcmp(avc.sps[0].data, sps2 + 4, sizeof(sps2) - 4) && 0 == memcmp(avc.sps[1].data, sps1 + 4, sizeof(sps1) - 4) && 1 == avc.nb_pps && avc.pps[0].bytes == sizeof(pps2) - 4 && 0 == memcmp(avc.pps[0].data, pps2 + 4, sizeof(pps2) - 4));
}

void mpeg4_annexbtomp4_test(void)
{
	const uint8_t sps[] = { 0x67,0x42,0xe0,0x1e,0xab };
	const uint8_t pps[] = { 0x28,0xce,0x3c,0x80 };
	const uint8_t annexb[] = { 0x00,0x00,0x00,0x01,0x67,0x42,0xe0,0x1e,0xab, 0x00,0x00,0x00,0x01,0x28,0xce,0x3c,0x80,0x00,0x00,0x00,0x01,0x65,0x11 };
	uint8_t output[256];
	int vcl, update;

	struct mpeg4_avc_t avc;
	memset(&avc, 0, sizeof(avc));
	assert(h264_annexbtomp4(&avc, annexb, sizeof(annexb), output, sizeof(output), &vcl, &update) > 0);
	assert(1 == avc.nb_sps && avc.sps[0].bytes == sizeof(sps) && 0 == memcmp(avc.sps[0].data, sps, sizeof(sps)));
	assert(1 == avc.nb_pps && avc.pps[0].bytes == sizeof(pps) && 0 == memcmp(avc.pps[0].data, pps, sizeof(pps)));
	assert(vcl == 1);

	mpeg4_annexbtomp4_test2();
}
#endif
