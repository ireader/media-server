#include "ctypedef.h"
#include <memory.h>
#include <assert.h>

static int h264_start_code(const uint8_t* h264)
{
	if (0 == h264[0] && 0 == h264[1] && (1 == h264[2] || (0 == h264[2] && 1 == h264[3])))
		return 1;
	return 0;
}

/*
ISO/IEC 14496-15:2010(E) 5.2.4.1.1 Syntax (p16)

aligned(8) class AVCDecoderConfigurationRecord {
	unsigned int(8) configurationVersion = 1;
	unsigned int(8) AVCProfileIndication;
	unsigned int(8) profile_compatibility;
	unsigned int(8) AVCLevelIndication;
	bit(6) reserved = ¡®111111¡¯b;
	unsigned int(2) lengthSizeMinusOne;
	bit(3) reserved = ¡®111¡¯b;

	unsigned int(5) numOfSequenceParameterSets;
	for (i=0; i< numOfSequenceParameterSets; i++) {
		unsigned int(16) sequenceParameterSetLength ;
		bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit;
	}

	unsigned int(8) numOfPictureParameterSets;
	for (i=0; i< numOfPictureParameterSets; i++) {
		unsigned int(16) pictureParameterSetLength;
		bit(8*pictureParameterSetLength) pictureParameterSetNALUnit;
	}

	if( profile_idc == 100 || profile_idc == 110 ||
		profile_idc == 122 || profile_idc == 144 )
	{
		bit(6) reserved = ¡®111111¡¯b;
		unsigned int(2) chroma_format;
		bit(5) reserved = ¡®11111¡¯b;
		unsigned int(3) bit_depth_luma_minus8;
		bit(5) reserved = ¡®11111¡¯b;
		unsigned int(3) bit_depth_chroma_minus8;
		unsigned int(8) numOfSequenceParameterSetExt;
		for (i=0; i< numOfSequenceParameterSetExt; i++) {
			unsigned int(16) sequenceParameterSetExtLength;
			bit(8*sequenceParameterSetExtLength) sequenceParameterSetExtNALUnit;
		}
	}
}
*/
int AVCDecoderConfigurationRecord(const uint8_t* data, uint32_t bytes, uint8_t* h264)
{
	uint8_t i;
	uint8_t sps, pps;
	uint16_t len;
	uint32_t j, k;

	//uint8_t version = data[0];
	//uint8_t profile = data[1];
	//uint8_t flags = data[2];
	//uint8_t level = data[3];
	//uint8_t nalu = (data[4] & 0x03) + 1;

	assert(1 == data[0]);
	if (bytes < 7)
		return -1;

	j = 5;
	k = 0;
	sps = data[j++] & 0x1F;
	for (i = 0; i < sps && j + 2 < bytes; i++)
	{
		len = (data[j] << 8) | data[j + 1];
		if (len + j + 2 >= bytes) // data length + pps length
			return -1;

		if (j + 6 <= bytes && 0 == h264_start_code(data + j + 2))
		{
			h264[k++] = 0;
			h264[k++] = 0;
			h264[k++] = 0;
			h264[k++] = 1;
		}
		memcpy(h264 + k, data + j + 2, len);
		k += len;
		j += len + 2;
	}

	pps = data[j++];
	for (i = 0; i < pps && j + 2 < bytes; i++)
	{
		len = (data[j] << 8) | data[j + 1];
		if (len + j + 2 > bytes)
			return -1;

		if (j + 6 <= bytes && 0 == h264_start_code(data + j + 2))
		{
			h264[k++] = 0;
			h264[k++] = 0;
			h264[k++] = 0;
			h264[k++] = 1;
		}
		memcpy(h264 + k, data + j + 2, len);
		k += len;
		j += len + 2;
	}

	return k;
}
