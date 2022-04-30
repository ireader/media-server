#ifndef _rtsp_payloads_h_
#define _rtsp_payloads_h_

#include "flv-proto.h"
#include "mov-format.h"
#include "mkv-format.h"
#include "rtp-profile.h"
#include "mpeg-ts-proto.h"
#include <stdlib.h>
#include <string.h>

#if defined(OS_WINDOWS)
    #if !defined(strcasecmp)
        #define strcasecmp	_stricmp
    #endif
#endif

#ifdef _avcodecid_h_
#define CODEC(avcodec, mov, mkv, flv, mpeg2, rtp, rate, encoding, name) { avcodec, flv, mov, mpeg2, rtp, encoding, mkv, rate, name }
#else
#define CODEC(avcodec, mov, mkv, flv, mpeg2, rtp, rate, encoding, name) { flv, mov, mpeg2, rtp, encoding, mkv, rate, name }
#endif

static struct
{
#ifdef _avcodecid_h_
    AVPACKET_CODEC_ID codecid;
#endif
    uint16_t flv; // flv codec id
    uint8_t mov; // mov object id
    uint8_t mpeg2; // mpeg2 codecid
    uint8_t payload; // rtp payload id
    const char* encoding; // rtp encoding
    enum mkv_codec_t mkv; // mkv codec id
    int frequency;

    const char* name; // describe
} s_payloads[] = {
    // video
    CODEC(AVCODEC_VIDEO_H264,   MOV_OBJECT_H264,    MKV_CODEC_VIDEO_H264,   FLV_VIDEO_H264,     PSI_STREAM_H264,        RTP_PAYLOAD_H264,   90000,  "H264",     "H264"),
    CODEC(AVCODEC_VIDEO_H265,   MOV_OBJECT_H265,    MKV_CODEC_VIDEO_H265,   FLV_VIDEO_H265,     PSI_STREAM_H265,        RTP_PAYLOAD_H265,   90000,  "H265",     "H265"),
    CODEC(AVCODEC_VIDEO_H265,   MOV_OBJECT_H265,    MKV_CODEC_VIDEO_H265,   FLV_VIDEO_H265,     PSI_STREAM_H265,        RTP_PAYLOAD_H265,   90000,  "HEVC",     "H265"),
    CODEC(AVCODEC_VIDEO_MPEG4,  MOV_OBJECT_MP4V,    MKV_CODEC_VIDEO_MPEG4,  0,                  PSI_STREAM_MPEG4,       RTP_PAYLOAD_MP4V,   90000,  "MP4V-ES",  "MPEG4"),
    CODEC(AVCODEC_VIDEO_MPEG4,  MOV_OBJECT_MP4V,    MKV_CODEC_VIDEO_MPEG4,  0,                  PSI_STREAM_MPEG4,       RTP_PAYLOAD_MP4V,   90000,  "MPEG4",    "MPEG4"),
    CODEC(AVCODEC_VIDEO_VP8,    MOV_OBJECT_VP8,     MKV_CODEC_VIDEO_VP8,    0,                  PSI_STREAM_VP8,         RTP_PAYLOAD_VP8,    90000,  "VP8",      "VP8"),
    CODEC(AVCODEC_VIDEO_VP9,    MOV_OBJECT_VP9,     MKV_CODEC_VIDEO_VP9,    0,                  PSI_STREAM_VP9,         RTP_PAYLOAD_VP9,    90000,  "VP9",      "VP9"),
    CODEC(AVCODEC_VIDEO_AV1,    MOV_OBJECT_AV1,     MKV_CODEC_VIDEO_AV1,    FLV_VIDEO_AV1,      PSI_STREAM_AV1,         RTP_PAYLOAD_AV1,    90000,  "AV1",      "AV1"),
    CODEC(AVCODEC_VIDEO_AV1,    MOV_OBJECT_AV1,     MKV_CODEC_VIDEO_AV1,    FLV_VIDEO_AV1,      PSI_STREAM_AV1,         RTP_PAYLOAD_AV1X,   90000,  "AV1X",     "AV1"), // https://bugs.chromium.org/p/webrtc/issues/detail?id=11042

    // audio
    CODEC(AVCODEC_AUDIO_AAC,    MOV_OBJECT_AAC,     MKV_CODEC_AUDIO_AAC,    FLV_AUDIO_AAC,      PSI_STREAM_AAC,         RTP_PAYLOAD_LATM,   44100,  "MP4A-LATM","AAC"),
    CODEC(AVCODEC_AUDIO_AAC,    MOV_OBJECT_AAC,     MKV_CODEC_AUDIO_AAC,    FLV_AUDIO_AAC,      PSI_STREAM_AAC,         RTP_PAYLOAD_MP4A,   44100,  "mpeg4-generic", "AAC"),
    CODEC(AVCODEC_AUDIO_OPUS,   MOV_OBJECT_OPUS,    MKV_CODEC_AUDIO_OPUS,   FLV_AUDIO_OPUS,     PSI_STREAM_AUDIO_OPUS,  RTP_PAYLOAD_OPUS,   48000,  "opus",     "opus"),
    CODEC(AVCODEC_AUDIO_MP3,    MOV_OBJECT_MP3,     MKV_CODEC_AUDIO_MP3,    FLV_AUDIO_MP3,      PSI_STREAM_MP3,         RTP_PAYLOAD_MP3,    16000,  "mp3",      "mp3"), // rtp standard payload id
    CODEC(AVCODEC_AUDIO_G711A,  MOV_OBJECT_G711a,   MKV_CODEC_UNKNOWN,      FLV_AUDIO_G711A,    PSI_STREAM_AUDIO_G711A, RTP_PAYLOAD_PCMA,   8000,   "pcma",     "g711a"), // rtp standard payload id
    CODEC(AVCODEC_AUDIO_G711U,  MOV_OBJECT_G711u,   MKV_CODEC_UNKNOWN,      FLV_AUDIO_G711U,    PSI_STREAM_AUDIO_G711U, RTP_PAYLOAD_PCMU,   8000,   "pcmu",     "g711u"), // rtp standard payload id

    // data
    CODEC(AVCODEC_DATA_RAW,     MOV_OBJECT_NONE,   MKV_CODEC_UNKNOWN,      FLV_SCRIPT_METADATA, PSI_STREAM_RESERVED,    RTP_PAYLOAD_DYNAMIC,0,      "raw",      "raw"),
    CODEC(AVCODEC_DATA_MP2P,    MOV_OBJECT_NONE,   MKV_CODEC_UNKNOWN,      0,                   PSI_STREAM_RESERVED,    RTP_PAYLOAD_MP2P,   90000,  "MP2P",     "PS"),
    
    // PS/TS
    CODEC(AVCODEC_NONE,         MOV_OBJECT_NONE,    MKV_CODEC_UNKNOWN,      0,                  PSI_STREAM_RESERVED,    RTP_PAYLOAD_MP2T,   90000,  "TS",       "TS"), // rtp standard payload id    
    CODEC(AVCODEC_NONE,         MOV_OBJECT_NONE,    MKV_CODEC_UNKNOWN,      0,                  PSI_STREAM_RESERVED,    RTP_PAYLOAD_MP2P,   90000,  "MP2P",     "PS"),
    CODEC(AVCODEC_NONE,         MOV_OBJECT_NONE,    MKV_CODEC_UNKNOWN,      0,                  PSI_STREAM_RESERVED,    RTP_PAYLOAD_MP2P,   90000,  "PS",       "PS"), // GB28181
};

#ifdef _avcodecid_h_
static inline int avcodecid_find_by_codecid(AVPACKET_CODEC_ID codecid)
{
    size_t i;
    for (i = 0; i < sizeof(s_payloads) / sizeof(s_payloads[0]); i++)
    {
        if (s_payloads[i].codecid == codecid && AVCODEC_NONE != s_payloads[i].codecid)
            return (int)i;
    }

    return -1;
}
#endif

static inline int avpayload_find_by_flv(int flv)
{
    size_t i;
    for (i = 0; i < sizeof(s_payloads) / sizeof(s_payloads[0]); i++)
    {
        if ((int)s_payloads[i].flv == flv && flv != 0)
            return (int)i;
    }

    return -1;
}

static inline int avpayload_find_by_mov(uint8_t object)
{
    size_t i;
    for (i = 0; i < sizeof(s_payloads) / sizeof(s_payloads[0]); i++)
    {
        if (s_payloads[i].mov == object && MOV_OBJECT_NONE != object)
            return (int)i;
    }

    return -1;
}

static inline int avpayload_find_by_mkv(enum mkv_codec_t codec)
{
    size_t i;
    for (i = 0; i < sizeof(s_payloads) / sizeof(s_payloads[0]); i++)
    {
        if (s_payloads[i].mkv == codec && MKV_CODEC_UNKNOWN != codec)
            return (int)i;
    }

    return -1;
}
static inline int avpayload_find_by_mpeg2(uint8_t mpeg2)
{
    size_t i;
    for (i = 0; i < sizeof(s_payloads) / sizeof(s_payloads[0]); i++)
    {
        if (s_payloads[i].mpeg2 == mpeg2 && PSI_STREAM_RESERVED != mpeg2)
            return (int)i;
    }

    return -1;
}

// for rtp encoding
static inline int avpayload_find_by_payload(uint8_t payload)
{
    size_t i;
    for (i = 0; i < sizeof(s_payloads) / sizeof(s_payloads[0]); i++)
    {
        if (s_payloads[i].payload == payload)
            return (int)i;
    }

    return -1;
}

// for rtp decoding
static inline int avpayload_find_by_rtp(uint8_t payload, const char* encoding)
{
    size_t i;
    for (i = 0; i < sizeof(s_payloads) / sizeof(s_payloads[0]); i++)
    {
        if ( (payload < RTP_PAYLOAD_DYNAMIC || !encoding || !*encoding) && s_payloads[i].payload == payload)
            return (int)i;
        else if (payload >= RTP_PAYLOAD_DYNAMIC && encoding && 0 == strcasecmp(encoding, s_payloads[i].encoding))
            return (int)i;
    }

    return -1;
}

#endif /* !_rtsp_payloads_h_ */
