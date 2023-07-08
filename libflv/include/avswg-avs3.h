#ifndef _avswg_avs3_h_
#define _avswg_avs3_h_

// http://standard.avswg.org.cn/avs3_download/

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct avswg_avs3_t
{
	uint32_t version : 8;
	uint32_t sequence_header_length : 16;
	uint32_t library_dependency_idc : 2;

	uint8_t sequence_header[2 * 1024]; // bytes: sequence_header_length
};
/// Create avs3 codec configuration record from bitstream
/// @param[in] data avs3 bitstream format(00 00 01 B0)
/// @return 0-ok, other-error
int avswg_avs3_decoder_configuration_record_init(struct avswg_avs3_t* avs3, const void* data, size_t bytes);

// load avs3 from Avs3DecoderConfigurationRecord
int avswg_avs3_decoder_configuration_record_load(const uint8_t* data, size_t bytes, struct avswg_avs3_t* avs3);

int avswg_avs3_decoder_configuration_record_save(const struct avswg_avs3_t* avs3, uint8_t* data, size_t bytes);

int avswg_avs3_codecs(const struct avswg_avs3_t* avs3, char* codecs, size_t bytes);

#if defined(__cplusplus)
}
#endif

#endif /* !_avswg_avs3_h_ */
