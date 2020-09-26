#ifndef _mpeg_ts_opus_h_
#define _mpeg_ts_opus_h_

#include <stdint.h>

// https://wiki.xiph.org/OpusTS
// https://people.xiph.org/~tterribe/opus/ETSI_TS_opus-v0.1.3-draft.doc (death link)
// https://patchwork.ffmpeg.org/project/ffmpeg/patch/5e3b1495-22de-346f-ab85-f022739c73a3@gmail.com/
/*
 *                   Table 4-2 opus_audio_descriptor syntax

 |Syntax                                 |Number of bits            |Identif|
 |                                       |                          |ier    |
 |opus_audio_descriptor() {              |                          |       |
 |   descriptor_tag                      |8                         |uimsbf |
 |   descriptor_length                   |8                         |uimsbf |
 |   channel_config_code                 |8                         |uimsbf |
 |   if(channel_config_code==0x81) {     |                          |       |
 |      channel_count                    |8                         |uimsbf |
 |      mapping_family                   |8                         |uimsbf |
 |      if(mapping_family>0) {           |                          |       |
 |         stream_count_minus_one        |ceil(log2(channel_count)) |uimsbf |
 |         coupled_stream_count          |ceil(log2(stream_count+1))|uimsbf |
 |         for(i=0; i<channel_count; i++)|                          |       |
 |{                                      |                          |       |
 |            channel_mapping[i]         |ceil(log2(stream_count    |uimsbf |
 |                                       |+coupled_stream_count+1)  |       |
 |         }                             |                          |       |
 |            reserved                   |N1                        |bsmsbf |
 |      }                                |                          |       |
 |   }                                   |                          |       |
 |}                                      |                          |       |
 */

// ISO/IEC 13818-1:2018 (E) Table 2-108 - Extension descriptor tag values (p132)
#define OPUS_EXTENSION_DESCRIPTOR_TAG 0x80

static const uint8_t opus_default_extradata[30] = {
    'O', 'p', 'u', 's', 'H', 'e', 'a', 'd',
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

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

#endif /* !_mpeg_ts_opus_h_ */
