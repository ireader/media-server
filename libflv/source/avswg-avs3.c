#include "avswg-avs3.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define AVS3_VIDEO_SEQUENCE_START 0xB0

/*
aligned(8) class Avs3DecoderConfigurationRecord {
	unsigned int(8) configurationVersion = 1;
	unsigned int(16) sequence_header_length;
	bit(8*sequence_header_length) sequence_header;
	bit(6) reserved = '111111'b;
	unsigned int(2) library_dependency_idc;
}
*/

int avswg_avs3_decoder_configuration_record_load(const uint8_t* data, size_t bytes, struct avswg_avs3_t* avs3)
{
	if (bytes < 4) return -1;
	assert(1 == data[0]);
	avs3->version = data[0];
	avs3->sequence_header_length = (((uint32_t)data[1]) << 8) | data[2];
	if (avs3->sequence_header_length + 4 > bytes || avs3->sequence_header_length > sizeof(avs3->sequence_header))
		return -1;

	memcpy(avs3->sequence_header, data + 3, avs3->sequence_header_length);
	avs3->library_dependency_idc = data[avs3->sequence_header_length + 3] & 0x03;
	return avs3->sequence_header_length + 4;
}

int avswg_avs3_decoder_configuration_record_save(const struct avswg_avs3_t* avs3, uint8_t* data, size_t bytes)
{
	if (bytes < 4 + avs3->sequence_header_length) return -1;

	data[0] = 1; // configurationVersion
	data[1] = (uint8_t)(avs3->sequence_header_length >> 8);
	data[2] = (uint8_t)(avs3->sequence_header_length);
	memcpy(data + 3, avs3->sequence_header, avs3->sequence_header_length);
	data[3 + avs3->sequence_header_length] = 0xFC | (uint8_t)(avs3->library_dependency_idc);
	return (int)(4 + avs3->sequence_header_length);
}

int avswg_avs3_codecs(const struct avswg_avs3_t* avs3, char* codecs, size_t bytes)
{
	// // AVS3-P6: Annex-A
	return snprintf(codecs, bytes, "avs3.%02x.%02x", (unsigned int)(avs3->sequence_header_length > 6 ? avs3->sequence_header[4] : 0), (unsigned int)(avs3->sequence_header_length > 6 ? avs3->sequence_header[5] : 0));
}

int avswg_avs3_decoder_configuration_record_init(struct avswg_avs3_t* avs3, const void* data, size_t bytes)
{
	size_t i;
	const uint8_t* p;

	p = data;
	if (bytes < 8 || 0 != p[0] || 0 != p[1] || 1 != p[2] || AVS3_VIDEO_SEQUENCE_START != p[3])
		return -1;

	for (i = 0; i + 1 < sizeof(avs3->sequence_header)
		&& (i < 4 || i + 3 >= bytes || 0 != p[i] || 0 != p[i + 1] || 1 != p[i + 2])
		; i++)
	{
		avs3->sequence_header[i] = p[i];
	}

	avs3->version = 1;
	avs3->sequence_header_length = (uint32_t)i;
	avs3->library_dependency_idc = 0;
	return 0;
}

#if defined(_DEBUG) || defined(DEBUG)
void avswg_avs3_test(void)
{
	const unsigned char src[] = {
		0x01,0x00,0x1d,0x00,0x00,0x01,0xb0,0x20,0x44,0x88,0xf0,0x11,0x0e,0x13,0x16,0x87,0x2b,
		0x10,0x00,0x20,0x10,0xcf,0xcf,0xc1,0x06,0x14,0x10,0x10,0x67,0x0f,0x04,0x48,0xfc,
	};
	unsigned char data[sizeof(src)];

	struct avswg_avs3_t avs3;
	assert(sizeof(src) == avswg_avs3_decoder_configuration_record_load(src, sizeof(src), &avs3));
	assert(1 == avs3.version && 0x1d == avs3.sequence_header_length && 0 == avs3.library_dependency_idc);
	assert(sizeof(src) == avswg_avs3_decoder_configuration_record_save(&avs3, data, sizeof(data)));
	assert(0 == memcmp(src, data, sizeof(src)));

	avswg_avs3_codecs(&avs3, (char*)data, sizeof(data));
	assert(0 == memcmp("avs3.20.44", data, 10));
}
#endif
