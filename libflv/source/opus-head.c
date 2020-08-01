#include "opus-head.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// http://www.opus-codec.org/docs/opus_in_isobmff.html
// 4.3.2 Opus Specific Box
/*
class ChannelMappingTable (unsigned int(8) OutputChannelCount){
    unsigned int(8) StreamCount;
    unsigned int(8) CoupledCount;
    unsigned int(8 * OutputChannelCount) ChannelMapping;
}

aligned(8) class OpusSpecificBox extends Box('dOps'){
    unsigned int(8) Version;
    unsigned int(8) OutputChannelCount;
    unsigned int(16) PreSkip;
    unsigned int(32) InputSampleRate;
    signed int(16) OutputGain;
    unsigned int(8) ChannelMappingFamily;
    if (ChannelMappingFamily != 0) {
        ChannelMappingTable(OutputChannelCount);
    }
}
*/

static const uint8_t opus_coupled_stream_cnt[9] = {
    1, 0, 1, 1, 2, 2, 2, 3, 3
};

static const uint8_t opus_stream_cnt[9] = {
    1, 1, 1, 2, 2, 3, 4, 4, 5,
};

static const uint8_t opus_channel_map[8][8] = {
    { 0 },
    { 0,1 },
    { 0,2,1 },
    { 0,1,2,3 },
    { 0,4,1,2,3 },
    { 0,4,1,2,3,5 },
    { 0,4,1,2,3,5,6 },
    { 0,6,1,2,3,4,5,7 },
};

int opus_head_save(const struct opus_head_t* opus, uint8_t* data, size_t bytes)
{
    if (bytes < 19)
        return -1;

    memcpy(data, "OpusHead", 8);
    data[8] = opus->version; // 0 only
    data[9] = opus->channels;
    data[11] = (uint8_t)(opus->pre_skip >> 8); // LSB
    data[10] = (uint8_t)opus->pre_skip;
    data[15] = (uint8_t)(opus->input_sample_rate >> 24); // LSB
    data[14] = (uint8_t)(opus->input_sample_rate >> 16);
    data[13] = (uint8_t)(opus->input_sample_rate >> 8);
    data[12] = (uint8_t)opus->input_sample_rate;
    data[17] = (uint8_t)(opus->output_gain >> 8); // LSB
    data[16] = (uint8_t)opus->output_gain;
    data[18] = opus->channel_mapping_family;
    if (0 != opus->channel_mapping_family && bytes >= 29)
    {
        data[19] = opus->stream_count;
        data[20] = opus->coupled_count;
        memcpy(data+21, opus->channel_mapping, 8);
        return 29;
    }

    return 19;
}

int opus_head_load(const uint8_t* data, size_t bytes, struct opus_head_t* opus)
{
    if (bytes < 19 || 0 != memcmp(data, "OpusHead", 8))
        return -1;

    memset(opus, 0, sizeof(*opus));
    opus->version = data[8];
    opus->channels = data[9];
    opus->pre_skip = ((uint16_t)data[11] << 8) | data[10];
    opus->input_sample_rate = ((uint32_t)data[15] << 24) | ((uint32_t)data[14] << 16) | ((uint32_t)data[13] << 8) | data[12];
    opus->output_gain = ((uint16_t)data[17] << 8) | data[16];
    opus->channel_mapping_family = data[18];

    if (0 != opus->channel_mapping_family && bytes >= 29)
    {
        opus->stream_count = data[19];
        opus->coupled_count = data[20];
        memcpy(opus->channel_mapping, data+21, 8);
        return 29;
    }
    else
    {
        opus->stream_count = opus_stream_cnt[opus->channels];
        opus->coupled_count = opus_coupled_stream_cnt[opus->channels];
        memcpy(opus->channel_mapping, opus_channel_map[opus_head_channels(opus)-1], 8);
    }

    return 19;
}

#if defined(DEBUG) || defined(_DEBUG)
void opus_head_test(void)
{
    uint8_t data[29];
    const uint8_t src[] = { 0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64, 0x01, 0x02, 0x78, 0x00, 0x80, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00 };

    struct opus_head_t opus;
    assert(sizeof(src) == opus_head_load(src, sizeof(src), &opus));
    assert(1 == opus.version && 2 == opus.channels && 120 == opus.pre_skip && 48000 == opus.input_sample_rate && 0 == opus.output_gain);
    assert(0 == opus.channel_mapping_family && 1 == opus.stream_count && 1 == opus.coupled_count);
    assert(0 == memcmp(opus_channel_map[opus.channels-1], opus.channel_mapping, 8));
    assert(sizeof(src) == opus_head_save(&opus, data, sizeof(data)));
    assert(0 == memcmp(src, data, sizeof(src)));
}
#endif
