#include "webm-vpx.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

enum {
    WEBM_VP_LEVEL_1     = 10,
    WEBM_VP_LEVEL_1_1   = 11,
    WEBM_VP_LEVEL_2     = 20,
    WEBM_VP_LEVEL_2_1   = 21,
    WEBM_VP_LEVEL_3     = 30,
    WEBM_VP_LEVEL_3_1   = 31,
    WEBM_VP_LEVEL_4     = 40,
    WEBM_VP_LEVEL_4_1   = 41,
    WEBM_VP_LEVEL_5     = 50,
    WEBM_VP_LEVEL_5_1   = 51,
    WEBM_VP_LEVEL_5_2   = 52,
    WEBM_VP_LEVEL_6     = 60,
    WEBM_VP_LEVEL_6_1   = 61,
    WEBM_VP_LEVEL_6_2   = 62,
};

/*
aligned (8) class VPCodecConfigurationRecord {
    unsigned int (8)     profile;
    unsigned int (8)     level;
    unsigned int (4)     bitDepth;
    unsigned int (3)     chromaSubsampling;
    unsigned int (1)     videoFullRangeFlag;
    unsigned int (8)     colourPrimaries;
    unsigned int (8)     transferCharacteristics;
    unsigned int (8)     matrixCoefficients;
    unsigned int (16)    codecIntializationDataSize;
    unsigned int (8)[]   codecIntializationData;
}
*/

int webm_vpx_codec_configuration_record_load(const uint8_t* data, size_t bytes, struct webm_vpx_t* vpx)
{
    if (bytes < 8)
        return -1;

    vpx->profile = data[0];
    vpx->level = data[1];
    vpx->bit_depth = (data[2] >> 4) & 0x0F;
    vpx->chroma_subsampling = (data[2] >> 1) & 0x07;
    vpx->video_full_range_flag = data[2] & 0x01;
    vpx->colour_primaries = data[3];
    vpx->transfer_characteristics = data[4];
    vpx->matrix_coefficients = data[5];
    vpx->codec_intialization_data_size = (((uint16_t)data[6]) << 8) | data[7];
    assert(0 == vpx->codec_intialization_data_size);
    return 8;
}

int webm_vpx_codec_configuration_record_save(const struct webm_vpx_t* vpx, uint8_t* data, size_t bytes)
{
    if (bytes < 8 + (size_t)vpx->codec_intialization_data_size)
        return 0; // don't have enough memory

    data[0] = vpx->profile;
    data[1] = vpx->level;
    data[2] = (vpx->bit_depth << 4) | ((vpx->chroma_subsampling & 0x07) << 1) | (vpx->video_full_range_flag & 0x01);
    data[3] = vpx->colour_primaries;
    data[4] = vpx->transfer_characteristics;
    data[5] = vpx->matrix_coefficients;
    data[6] = (uint8_t)(vpx->codec_intialization_data_size >> 8);
    data[7] = (uint8_t)vpx->codec_intialization_data_size;

    if(vpx->codec_intialization_data_size > 0)
        memcpy(data + 8, vpx->codec_intialization_data, vpx->codec_intialization_data_size);
    return 8 + vpx->codec_intialization_data_size;
}

#if defined(_DEBUG) || defined(DEBUG)
void webm_vpx_test(void)
{
    const unsigned char src[] = {
        0x00, 0x1f, 0x80, 0x02, 0x02, 0x02, 0x00, 0x00
    };
    unsigned char data[sizeof(src)];

    struct webm_vpx_t vpx;
    assert(sizeof(src) == webm_vpx_codec_configuration_record_load(src, sizeof(src), &vpx));
    assert(0 == vpx.profile && 31 == vpx.level && 8 == vpx.bit_depth && 0 == vpx.chroma_subsampling && 0 == vpx.video_full_range_flag);
    assert(sizeof(src) == webm_vpx_codec_configuration_record_save(&vpx, data, sizeof(data)));
    assert(0 == memcmp(src, data, sizeof(src)));
}
#endif
