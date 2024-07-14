#include "mpeg4-vvc.h"
#include "mpeg4-bits.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define H266_OPI		12
#define H266_DCI		13
#define H266_VPS		14
#define H266_SPS		15
#define H266_PPS		16
#define H266_AUD		20
#define H266_PREFIX_SEI 23
#define H266_SUFFIX_SEI 24

/*
* ISO/IEC 14496-15:2021 11.2.4.2.2 Syntax (p156)

aligned(8) class VvcPTLRecord(num_sublayers) {
	bit(2) reserved = 0;
	unsigned int(6) num_bytes_constraint_info;
	unsigned int(7) general_profile_idc;
	unsigned int(1) general_tier_flag;
	unsigned int(8) general_level_idc;
	unsigned int(1) ptl_frame_only_constraint_flag;
	unsigned int(1) ptl_multi_layer_enabled_flag;
	unsigned int(8*num_bytes_constraint_info - 2) general_constraint_info;
	for (i=num_sublayers - 2; i >= 0; i--)
		unsigned int(1) ptl_sublayer_level_present_flag[i];
	for (j=num_sublayers; j<=8 && num_sublayers > 1; j++)
		bit(1) ptl_reserved_zero_bit = 0;
	for (i=num_sublayers-2; i >= 0; i--)
		if (ptl_sublayer_level_present_flag[i])
			unsigned int(8) sublayer_level_idc[i];
	unsigned int(8) ptl_num_sub_profiles;
	for (j=0; j < ptl_num_sub_profiles; j++)
		unsigned int(32) general_sub_profile_idc[j];
}
*/

static int mpeg4_vvc_ptl_record_load(struct mpeg4_bits_t* bits, struct mpeg4_vvc_t* vvc)
{
	int i;
	mpeg4_bits_read_n(bits, 2); // reserved
	vvc->native_ptl.num_bytes_constraint_info = mpeg4_bits_read_uint32(bits, 6);
	vvc->native_ptl.general_profile_idc = mpeg4_bits_read_uint32(bits, 7);
	vvc->native_ptl.general_tier_flag = mpeg4_bits_read_uint32(bits, 1);
	vvc->native_ptl.general_level_idc = mpeg4_bits_read_uint32(bits, 8);
	for (i = 0; i < (int)vvc->native_ptl.num_bytes_constraint_info && i < sizeof(vvc->native_ptl.general_constraint_info)/sizeof(vvc->native_ptl.general_constraint_info[0]); i++)
	{
		vvc->native_ptl.general_constraint_info[i] = mpeg4_bits_read_uint8(bits, 8);
	}
	vvc->native_ptl.ptl_frame_only_constraint_flag = (vvc->native_ptl.general_constraint_info[0] & 0x80) ? 1 : 0;
	vvc->native_ptl.ptl_multi_layer_enabled_flag = (vvc->native_ptl.general_constraint_info[0] & 0x40) ? 1 : 0;

	vvc->native_ptl.ptl_sublayer_level_present_flag = 0;
	assert(vvc->num_sublayers >= 0 && vvc->num_sublayers <= 8);
	for (i = (int)vvc->num_sublayers - 2; i >= 0; i-=8)
		vvc->native_ptl.ptl_sublayer_level_present_flag = mpeg4_bits_read_uint8(bits, 8);

	for (i = (int)vvc->num_sublayers - 2; i >= 0 && i < sizeof(vvc->native_ptl.sublayer_level_idc)/sizeof(vvc->native_ptl.sublayer_level_idc[0]); i--)
	{
		if(vvc->native_ptl.ptl_sublayer_level_present_flag & (1 << i))
			vvc->native_ptl.sublayer_level_idc[i] = mpeg4_bits_read_uint8(bits, 8);
	}

	vvc->native_ptl.ptl_num_sub_profiles = mpeg4_bits_read_uint8(bits, 8);
	vvc->native_ptl.general_sub_profile_idc = (uint32_t*)(vvc->data + vvc->off);
	vvc->off += 4 * vvc->native_ptl.ptl_num_sub_profiles;
	for (i = 0; i < vvc->native_ptl.ptl_num_sub_profiles; i++)
	{
		vvc->native_ptl.general_sub_profile_idc[i] = mpeg4_bits_read_uint32(bits, 32);
	}

	return mpeg4_bits_error(bits);
}

static int mpeg4_vvc_ptl_record_save(struct mpeg4_bits_t* bits, const struct mpeg4_vvc_t* vvc)
{
	int i;
	mpeg4_bits_write_n(bits, 0, 2); // reserved
	mpeg4_bits_write_n(bits, vvc->native_ptl.num_bytes_constraint_info, 6);
	mpeg4_bits_write_n(bits, vvc->native_ptl.general_profile_idc, 7);
	mpeg4_bits_write_n(bits, vvc->native_ptl.general_tier_flag, 1);
	mpeg4_bits_write_n(bits, vvc->native_ptl.general_level_idc, 8);
	mpeg4_bits_write_n(bits, vvc->native_ptl.ptl_frame_only_constraint_flag, 1);
	mpeg4_bits_write_n(bits, vvc->native_ptl.ptl_multi_layer_enabled_flag, 1);
	for (i = 0; i < (int)vvc->native_ptl.num_bytes_constraint_info; i++)
	{
		mpeg4_bits_write_n(bits, vvc->native_ptl.general_constraint_info[i], i + 1 < (int)vvc->native_ptl.num_bytes_constraint_info ? 8 : 6);
	}

	assert(vvc->num_sublayers >= 0 && vvc->num_sublayers <= 8);
	for (i = (int)vvc->num_sublayers - 2; i >= 0; i -= 8)
		mpeg4_bits_write_n(bits, vvc->native_ptl.ptl_sublayer_level_present_flag, 8);

	for (i = (int)vvc->num_sublayers - 2; i >= 0 && i < sizeof(vvc->native_ptl.sublayer_level_idc) / sizeof(vvc->native_ptl.sublayer_level_idc[0]); i--)
	{
		if (vvc->native_ptl.ptl_sublayer_level_present_flag & (1 << i))
			mpeg4_bits_write_uint8(bits, vvc->native_ptl.sublayer_level_idc[i], 8);
	}

	mpeg4_bits_write_uint8(bits, vvc->native_ptl.ptl_num_sub_profiles, 8);
	for (i = 0; i < vvc->native_ptl.ptl_num_sub_profiles; i++)
	{
		mpeg4_bits_write_uint32(bits, vvc->native_ptl.general_sub_profile_idc[i], 32);
	}

	return mpeg4_bits_error(bits);
}

/*
* ISO/IEC 14496-15:2021 11.2.4.2.2 Syntax (p156)

aligned(8) class VvcDecoderConfigurationRecord {
	bit(5) reserved = '11111'b;
	unsigned int(2) LengthSizeMinusOne;
	unsigned int(1) ptl_present_flag;
	if (ptl_present_flag) {
		unsigned int(9) ols_idx;
		unsigned int(3) num_sublayers;
		unsigned int(2) constant_frame_rate;
		unsigned int(2) chroma_format_idc;
		unsigned int(3) bit_depth_minus8;
		bit(5) reserved = '11111'b;
		VvcPTLRecord(num_sublayers) native_ptl;
		unsigned_int(16) max_picture_width;
		unsigned_int(16) max_picture_height;
		unsigned int(16) avg_frame_rate;
	}
	unsigned int(8) num_of_arrays;
	for (j=0; j < num_of_arrays; j++) {
		unsigned int(1) array_completeness;
		bit(2) reserved = 0;
		unsigned int(5) NAL_unit_type;
		if (NAL_unit_type != DCI_NUT  &&  NAL_unit_type != OPI_NUT)
			unsigned int(16) num_nalus;
		for (i=0; i< num_nalus; i++) {
			unsigned int(16) nal_unit_length;
			bit(8*nal_unit_length) nal_unit;
		}
	}
}
*/
int mpeg4_vvc_decoder_configuration_record_load(const uint8_t* data, size_t bytes, struct mpeg4_vvc_t* vvc)
{
	struct mpeg4_bits_t bits;
	uint8_t nalutype;
	uint16_t i, j, k, n, numOfArrays;
	uint8_t* dst;

	vvc->off = 0; // clear
	mpeg4_bits_init(&bits, (void*)data, bytes);
	mpeg4_bits_read_n(&bits, 5); // reserved '11111'b
	vvc->lengthSizeMinusOne = mpeg4_bits_read_uint32(&bits, 2);
	vvc->ptl_present_flag = mpeg4_bits_read(&bits);
	if (vvc->ptl_present_flag)
	{
		vvc->ols_idx = mpeg4_bits_read_uint32(&bits, 9);
		vvc->num_sublayers = mpeg4_bits_read_uint32(&bits, 3);
		vvc->constant_frame_rate = mpeg4_bits_read_uint32(&bits, 2);
		vvc->chroma_format_idc = mpeg4_bits_read_uint32(&bits, 2);
		vvc->bit_depth_minus8 = mpeg4_bits_read_uint32(&bits, 3);
		mpeg4_bits_read_n(&bits, 5); // reserved '11111'b
		mpeg4_vvc_ptl_record_load(&bits, vvc);
		vvc->max_picture_width = mpeg4_bits_read_uint16(&bits, 16);
		vvc->max_picture_height = mpeg4_bits_read_uint16(&bits, 16);
		vvc->avg_frame_rate = mpeg4_bits_read_uint16(&bits, 16);
	}

	if (0 != mpeg4_bits_error(&bits))
	{
		assert(0);
		return -1;
	}

	assert(0 == bits.bits % 8);
	dst = vvc->data + vvc->off;

	numOfArrays = mpeg4_bits_read_uint8(&bits, 8);
	for (i = 0; i < numOfArrays && 0 == mpeg4_bits_error(&bits); i++)
	{
		nalutype = mpeg4_bits_read_uint8(&bits, 8);
		
		n = 1;
		if ((nalutype & 0x1f) != H266_DCI && (nalutype & 0x1f) != H266_OPI)
			n = mpeg4_bits_read_uint16(&bits, 16);

		for (j = 0; j < n; j++)
		{
			if (vvc->numOfArrays >= sizeof(vvc->nalu) / sizeof(vvc->nalu[0]))
			{
				assert(0);
				return -E2BIG; // too many nalu(s)
			}

			k = mpeg4_bits_read_uint16(&bits, 16);
			vvc->nalu[vvc->numOfArrays].array_completeness = (nalutype >> 7) & 0x01;
			vvc->nalu[vvc->numOfArrays].type = nalutype & 0x1F;
			vvc->nalu[vvc->numOfArrays].bytes = k;
			vvc->nalu[vvc->numOfArrays].data = dst;
			memcpy(vvc->nalu[vvc->numOfArrays].data, data + bits.bits / 8, k);
			vvc->numOfArrays++;

			mpeg4_bits_skip(&bits, (uint64_t)k * 8);
			dst += k;
		}
	}

	vvc->off = (int)(dst - vvc->data);
	return mpeg4_bits_error(&bits) ? -1 : (int)(bits.bits / 8);
}

int mpeg4_vvc_decoder_configuration_record_save(const struct mpeg4_vvc_t* vvc, uint8_t* data, size_t bytes)
{
	uint16_t n;
	uint8_t i, j, k;
	uint8_t* ptr, * end;
	uint8_t* p;
	uint8_t array_completeness = 1;
	struct mpeg4_bits_t bits;
	const uint8_t nalu[] = { H266_OPI, H266_DCI, H266_VPS, H266_SPS, H266_PPS, H266_PREFIX_SEI, H266_SUFFIX_SEI };

	assert(vvc->lengthSizeMinusOne <= 3);
	memset(data, 0, bytes);
	mpeg4_bits_init(&bits, (void*)data, bytes);
	mpeg4_bits_write_n(&bits, 0x1F, 5);
	mpeg4_bits_write_n(&bits, vvc->lengthSizeMinusOne, 2);
	mpeg4_bits_write_n(&bits, vvc->ptl_present_flag, 1);

	if (vvc->ptl_present_flag)
	{
		mpeg4_bits_write_n(&bits, vvc->ols_idx, 9);
		mpeg4_bits_write_n(&bits, vvc->num_sublayers, 3);
		mpeg4_bits_write_n(&bits, vvc->constant_frame_rate, 2);
		mpeg4_bits_write_n(&bits, vvc->chroma_format_idc, 2);
		mpeg4_bits_write_n(&bits, vvc->bit_depth_minus8, 3);
		mpeg4_bits_write_n(&bits, 0x1F, 5);
		mpeg4_vvc_ptl_record_save(&bits, vvc);
		mpeg4_bits_write_uint16(&bits, vvc->max_picture_width, 16);
		mpeg4_bits_write_uint16(&bits, vvc->max_picture_height, 16);
		mpeg4_bits_write_uint16(&bits, vvc->avg_frame_rate, 16);
	}
	
	if (0 != mpeg4_bits_error(&bits) || 0 != bits.bits % 8)
	{
		assert(0);
		return -1;
	}

	//mpeg4_bits_write_uint8(&bits, vvc->numOfArrays, 8);
	p = data + bits.bits / 8 + 1 /*num_of_arrays*/;
	end = data + bytes;
	for (k = i = 0; i < sizeof(nalu) / sizeof(nalu[0]) && p + 5 <= end; i++)
	{
		ptr = p + 3;
		for (n = j = 0; j < vvc->numOfArrays; j++)
		{
			if (nalu[i] != vvc->nalu[j].type)
				continue;

			if (ptr + 2 + vvc->nalu[j].bytes > end)
				return 0; // don't have enough memory

			array_completeness = vvc->nalu[j].array_completeness;
			assert(vvc->nalu[i].data + vvc->nalu[j].bytes <= vvc->data + sizeof(vvc->data));
			ptr[0] = (vvc->nalu[j].bytes >> 8) & 0xFF;
			ptr[1] = vvc->nalu[j].bytes & 0xFF;
			memcpy(ptr + 2, vvc->nalu[j].data, vvc->nalu[j].bytes);
			ptr += 2 + vvc->nalu[j].bytes;
			n++;
		}

		if (n > 0)
		{
			// array_completeness + NAL_unit_type
			p[0] = (array_completeness << 7) | (nalu[i] & 0x1F);
			p[1] = (n >> 8) & 0xFF;
			p[2] = n & 0xFF;
			p = ptr;
			k++;
		}
	}

	data[bits.bits / 8] = k; // num_of_arrays

	return mpeg4_bits_error(&bits) ? -1 : (int)(p - data);
}

int mpeg4_vvc_to_nalu(const struct mpeg4_vvc_t* vvc, uint8_t* data, size_t bytes)
{
	uint8_t i;
	uint8_t* p, * end;
	const uint8_t startcode[] = { 0, 0, 0, 1 };

	p = data;
	end = p + bytes;

	for (i = 0; i < vvc->numOfArrays; i++)
	{
		if (p + vvc->nalu[i].bytes + 4 > end || vvc->nalu[i].bytes < 2)
			return -1;

		memcpy(p, startcode, 4);
		memcpy(p + 4, vvc->nalu[i].data, vvc->nalu[i].bytes);
		assert(vvc->nalu[i].type == ((vvc->nalu[i].data[1] >> 3) & 0x1F));
		p += 4 + vvc->nalu[i].bytes;
	}

	return (int)(p - data);
}

// RFC4648
static size_t base32_encode(char* target, const void* source, size_t bytes)
{
	size_t i, j;
	const uint8_t* ptr = (const uint8_t*)source;
	static const char* s_base32_enc = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

	for (j = i = 0; i < bytes / 5 * 5; i += 5)
	{
		target[j++] = s_base32_enc[(ptr[i] >> 3) & 0x1F]; /* c1 */
		target[j++] = s_base32_enc[((ptr[i] & 0x07) << 2) | ((ptr[i + 1] >> 6) & 0x03)]; /*c2*/
		target[j++] = s_base32_enc[(ptr[i + 1] >> 1) & 0x1F];/*c3*/
		target[j++] = s_base32_enc[((ptr[i + 1] & 0x01) << 4) | ((ptr[i + 2] >> 4) & 0x0F)]; /*c4*/
		target[j++] = s_base32_enc[((ptr[i + 2] & 0x0F) << 1) | ((ptr[i + 3] >> 7) & 0x01)]; /*c5*/
		target[j++] = s_base32_enc[(ptr[i + 3] >> 2) & 0x1F];/*c6*/
		target[j++] = s_base32_enc[((ptr[i + 3] & 0x03) << 3) | ((ptr[i + 4] >> 5) & 0x07)]; /*c7*/
		target[j++] = s_base32_enc[ptr[i + 4] & 0x1F]; /* c8 */
	}

	if (i + 1 == bytes)
	{
		target[j++] = s_base32_enc[(ptr[i] >> 3) & 0x1F]; /* c1 */
		target[j++] = s_base32_enc[((ptr[i] & 0x07) << 2)]; /*c2*/
	}
	else if (i + 2 == bytes)
	{
		target[j++] = s_base32_enc[(ptr[i] >> 3) & 0x1F]; /* c1 */
		target[j++] = s_base32_enc[((ptr[i] & 0x07) << 2) | ((ptr[i + 1] >> 6) & 0x03)]; /*c2*/
		target[j++] = s_base32_enc[(ptr[i + 1] >> 1) & 0x1F];/*c3*/
		target[j++] = s_base32_enc[((ptr[i + 1] & 0x01) << 4)]; /*c4*/
	}
	else if (i + 3 == bytes)
	{
		target[j++] = s_base32_enc[(ptr[i] >> 3) & 0x1F]; /* c1 */
		target[j++] = s_base32_enc[((ptr[i] & 0x07) << 2) | ((ptr[i + 1] >> 6) & 0x03)]; /*c2*/
		target[j++] = s_base32_enc[(ptr[i + 1] >> 1) & 0x1F];/*c3*/
		target[j++] = s_base32_enc[((ptr[i + 1] & 0x01) << 4) | ((ptr[i + 2] >> 4) & 0x0F)]; /*c4*/
		target[j++] = s_base32_enc[((ptr[i + 2] & 0x0F) << 1)]; /*c5*/
	}
	else if (i + 4 == bytes)
	{
		target[j++] = s_base32_enc[(ptr[i] >> 3) & 0x1F]; /* c1 */
		target[j++] = s_base32_enc[((ptr[i] & 0x07) << 2) | ((ptr[i + 1] >> 6) & 0x03)]; /*c2*/
		target[j++] = s_base32_enc[(ptr[i + 1] >> 1) & 0x1F];/*c3*/
		target[j++] = s_base32_enc[((ptr[i + 1] & 0x01) << 4) | ((ptr[i + 2] >> 4) & 0x0F)]; /*c4*/
		target[j++] = s_base32_enc[((ptr[i + 2] & 0x0F) << 1) | ((ptr[i + 3] >> 7) & 0x01)]; /*c5*/
		target[j++] = s_base32_enc[(ptr[i + 3] >> 2) & 0x1F];/*c6*/
		target[j++] = s_base32_enc[((ptr[i + 3] & 0x03) << 3)]; /*c7*/
	}

	while (0 != (j % 8))
	{
		target[j++] = '=';
	}

	return j;
}

int mpeg4_vvc_codecs(const struct mpeg4_vvc_t* vvc, char* codecs, size_t bytes)
{
	// ISO/IEC 14496-15:2021
	// Annex E Sub-parameters of the MIME type "codecs" parameter (p276)
	// 'vvc1.' or 'vvi1.' prefix (5 chars)
	int i, n;
	char buffer[129];

	// 1. trailing zero bits of the general_constraint_info() syntax structure may be omitted from the input bits to base32 encoding
	n = (int)vvc->native_ptl.num_bytes_constraint_info;
	for (i = (int)vvc->native_ptl.num_bytes_constraint_info - 1; i >= 0 && n > 1; i--)
	{
		if (0 == vvc->native_ptl.general_constraint_info[i])
			n--;
		else
			break;
	}
	i = base32_encode(buffer, vvc->native_ptl.general_constraint_info, n);
	//2, the trailing padding with the "=" character may be omitted from the base32 string;
	while (i > 0 && buffer[i - 1] == '=') i--;

	return snprintf(codecs, bytes, "vvc1.%u.%c%u.C%.*s",
		(unsigned int)vvc->native_ptl.general_profile_idc,
		vvc->native_ptl.general_tier_flag ? 'H' : 'L', vvc->native_ptl.general_level_idc, i, buffer);
}

#if defined(_DEBUG) || defined(DEBUG)
void vvc_annexbtomp4_test(void);
static void mpeg4_vvc_codecs_test(struct mpeg4_vvc_t* vvc)
{
	int r;
	char buffer[129];
	//const char* s = "vvc1.1.L51.CQA.O1+3";
	//const char* s1 = "vvc1.17.L83.CYA.O1+3";
	//const char* s2 = "vvc1.17.L83.CYA.O1+3";
	const char* s3 = "vvc1.1.L105.CAA";
	r = mpeg4_vvc_codecs(vvc, buffer, sizeof(buffer));
	assert(r == strlen(s3) && 0 == memcmp(buffer, s3, r));
}

void mpeg4_vvc_test(void)
{
	const uint8_t data[] = { 0xff, 0x00, 0x11, 0x1f, 0x01, 0x02, 0x69, 0x00, 0x00, 0x02, 0xd0, 0x05, 0x00, 0x00, 0x00, 0x02, 0x8f, 0x00, 0x01, 0x00, 0x2a, 0x00, 0x79, 0x00, 0x0b, 0x02, 0x69, 0x00, 0x00, 0x03, 0x00, 0x16, 0x88, 0x01, 0x40, 0x48, 0x80, 0x2b, 0x49, 0xff, 0x45, 0x19, 0x18, 0xe0, 0x0c, 0x42, 0x55, 0x5a, 0xab, 0xd5, 0xeb, 0x33, 0x25, 0x5a, 0x12, 0xe4, 0x72, 0xd4, 0x56, 0x5a, 0x32, 0x30, 0x40, 0x90, 0x00, 0x01, 0x00, 0x0c, 0x00, 0x81, 0x00, 0x00, 0x0b, 0x44, 0x00, 0xa0, 0x22, 0x24, 0x18, 0x20 };
	uint8_t buffer[sizeof(data)];
	struct mpeg4_vvc_t vvc;
	memset(&vvc, 0, sizeof(vvc));
	assert(sizeof(data) == mpeg4_vvc_decoder_configuration_record_load(data, sizeof(data), &vvc));
	assert(3 == vvc.lengthSizeMinusOne && 1 == vvc.ptl_present_flag && 1 == vvc.num_sublayers);
	assert(1 == vvc.chroma_format_idc && 0 == vvc.bit_depth_minus8);
	assert(720 == vvc.max_picture_width && 1280 == vvc.max_picture_height && 0 == vvc.avg_frame_rate);
	assert(1 == vvc.native_ptl.num_bytes_constraint_info && 1 == vvc.native_ptl.general_profile_idc && 0 == vvc.native_ptl.general_tier_flag && 0x69 == vvc.native_ptl.general_level_idc);
	assert(2 == vvc.numOfArrays && H266_SPS == vvc.nalu[0].type && 0x2a == vvc.nalu[0].bytes && H266_PPS == vvc.nalu[1].type && 0x0c == vvc.nalu[1].bytes);
	assert(sizeof(data) == mpeg4_vvc_decoder_configuration_record_save(&vvc, buffer, sizeof(buffer)) && 0 == memcmp(buffer, data, sizeof(data)));
	mpeg4_vvc_codecs_test(&vvc);
}
#endif
