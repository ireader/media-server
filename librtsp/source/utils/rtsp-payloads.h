#ifndef _rtsp_payloads_h_
#define _rtsp_payloads_h_

#include "mov-format.h"
#include "rtp-profile.h"
#include "mpeg-ts-proto.h"

static struct
{
    uint8_t mov; // mov object id
    uint8_t mpeg2; // mpeg2 codecid
    uint8_t payload; // rtp payload id
    const char* encoding; // rtp encoding
} s_payloads[] = {
    // video
    { MOV_OBJECT_H264,    PSI_STREAM_H264,       RTP_PAYLOAD_H264,   "H264", },
    { MOV_OBJECT_HEVC,    PSI_STREAM_H265,       RTP_PAYLOAD_H265,   "H265", },
    { MOV_OBJECT_VP8,     PSI_STREAM_VP8,        RTP_PAYLOAD_VP8,    "VP8", },
    { MOV_OBJECT_VP9,     PSI_STREAM_VP9,        RTP_PAYLOAD_VP9,    "VP9", },

    // audio
    { MOV_OBJECT_AAC,     PSI_STREAM_AAC,        RTP_PAYLOAD_MP4A,   "MP4A-LATM", },
    { MOV_OBJECT_OPUS,    PSI_STREAM_AUDIO_OPUS, RTP_PAYLOAD_OPUS,   "opus", },
    { MOV_OBJECT_MP3,     PSI_STREAM_MP3,        RTP_PAYLOAD_MP3,    "", }, // rtp standard payload id
    { MOV_OBJECT_G711u,   PSI_STREAM_AUDIO_G711U,RTP_PAYLOAD_PCMU,   "", }, // rtp standard payload id
    { MOV_OBJECT_G711a,   PSI_STREAM_AUDIO_G711A,RTP_PAYLOAD_PCMA,   "", }, // rtp standard payload id    
};

static inline int avpayload_find_by_mov(uint8_t object)
{
    int i;
    for (i = 0; i < sizeof(s_payloads) / sizeof(s_payloads[0]); i++)
    {
        if (s_payloads[i].mov == object)
            return i;
    }

    return -1;
}

static inline int avpayload_find_by_mpeg2(uint8_t mpeg2)
{
    int i;
    for (i = 0; i < sizeof(s_payloads) / sizeof(s_payloads[0]); i++)
    {
        if (s_payloads[i].mpeg2 == mpeg2)
            return i;
    }

    return -1;
}

static inline int avpayload_find_by_payload(uint8_t payload)
{
    int i;
    for (i = 0; i < sizeof(s_payloads) / sizeof(s_payloads[0]); i++)
    {
        if (s_payloads[i].payload == payload)
            return i;
    }

    return -1;
}

#endif /* !_rtsp_payloads_h_ */
