#include "sdp-payload.h"
#include "rtp-profile.h"
#include <assert.h>

int sdp_payload_video(uint8_t* data, int bytes, int rtp, const char* proto, unsigned short port, int payload, int frequence, const void* extra, int extra_size)
{
    switch (rtp)
    {
    case RTP_PAYLOAD_H264:
        return sdp_h264(data, bytes, proto, port, payload, frequence, extra, extra_size);

    case RTP_PAYLOAD_H265:
        return sdp_h265(data, bytes, proto, port, payload, frequence, extra, extra_size);

    case RTP_PAYLOAD_MP2T:
        return sdp_mpeg2_ts(data, bytes, proto, port);

    case RTP_PAYLOAD_MP2P:
        return sdp_mpeg2_ps(data, bytes, proto, port, payload);

    case RTP_PAYLOAD_VP8:
        return sdp_vp8(data, bytes, proto, port, payload);

    case RTP_PAYLOAD_VP9:
        return sdp_vp9(data, bytes, proto, port, payload);

    case RTP_PAYLOAD_AV1:
    case RTP_PAYLOAD_AV1X:
        return sdp_av1(data, bytes, proto, port, payload, frequence, extra, extra_size);

    case RTP_PAYLOAD_MP4ES:
        return sdp_mpeg4_es(data, bytes, proto, port, payload, frequence, extra, extra_size);

    default:
        assert(0);
        return -1;
    }
}

int sdp_payload_audio(uint8_t* data, int bytes, int rtp, const char* proto, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size)
{
    switch (rtp)
    {
    case RTP_PAYLOAD_LATM:
        return sdp_aac_latm(data, bytes, proto, port, payload, sample_rate, channel_count, extra, extra_size);
        
    case RTP_PAYLOAD_MP4A:
        return sdp_aac_generic(data, bytes, proto, port, payload, sample_rate, channel_count, extra, extra_size);

    case RTP_PAYLOAD_OPUS:
        return sdp_opus(data, bytes, proto, port, payload, sample_rate, channel_count, extra, extra_size);

    case RTP_PAYLOAD_PCMU:
        return sdp_g711u(data, bytes, proto, port);

    case RTP_PAYLOAD_PCMA:
        return sdp_g711a(data, bytes, proto, port);

    default:
        assert(0);
        return -1;
    }
}
