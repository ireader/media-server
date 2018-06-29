#ifndef _mpeg4_avc_h_
#define _mpeg4_avc_h_

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct mpeg4_avc_t
{
//	uint8_t version; // 1-only
	uint8_t profile;
	uint8_t compatibility; // constraint_set[0-5]_flag
	uint8_t level;
	uint8_t nalu; // NALUnitLength = (lengthSizeMinusOne + 1), default 4(0x03+1)

	uint8_t nb_sps;
	uint8_t nb_pps;

	struct mpeg4_avc_sps_t
	{
		uint16_t bytes;
		uint8_t* data;
	}  sps[32]; // [0-31]

	struct mpeg4_avc_pps_t
	{
		uint16_t bytes;
		uint8_t* data;
	} pps[256];

	// extension
	uint8_t chroma_format_idc;
	uint8_t bit_depth_luma_minus8;
	uint8_t bit_depth_chroma_minus8;

    uint8_t data[4 * 1024];
};

int mpeg4_avc_decoder_configuration_record_load(const uint8_t* data, size_t bytes, struct mpeg4_avc_t* avc);

int mpeg4_avc_decoder_configuration_record_save(const struct mpeg4_avc_t* avc, uint8_t* data, size_t bytes);

int mpeg4_avc_to_nalu(const struct mpeg4_avc_t* avc, uint8_t* data, size_t bytes);

int mpeg4_avc_codecs(const struct mpeg4_avc_t* avc, char* codecs, size_t bytes);

size_t mpeg4_annexbtomp4(struct mpeg4_avc_t* avc, const void* data, size_t bytes, void* out, size_t size);

size_t mpeg4_mp4toannexb(const struct mpeg4_avc_t* avc, const void* data, size_t bytes, void* out, size_t size);

#if defined(__cplusplus)
}
#endif
#endif /* !_mpeg4_avc_h_ */
