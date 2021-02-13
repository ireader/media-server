#include "rtp-sender.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include "rtp.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(OS_WINDOWS)
    #if !defined(strcasecmp)
        #define strcasecmp	_stricmp
    #endif
#endif

uint32_t rtp_ssrc(void);
int sdp_vp8(uint8_t* data, int bytes, unsigned short port, int payload);
int sdp_vp9(uint8_t* data, int bytes, unsigned short port, int payload);
int sdp_h264(uint8_t *data, int bytes, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
int sdp_h265(uint8_t *data, int bytes, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
int sdp_mpeg4_es(uint8_t *data, int bytes, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
int sdp_opus(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
int sdp_aac_latm(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
int sdp_aac_generic(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
int sdp_mpeg2_ps(uint8_t* data, int bytes, unsigned short port, int payload);
int sdp_mpeg2_ts(uint8_t* data, int bytes, unsigned short port);
int sdp_g711u(uint8_t *data, int bytes, unsigned short port);
int sdp_g711a(uint8_t *data, int bytes, unsigned short port);

static void* rtp_alloc(void* param, int bytes)
{
    struct rtp_sender_t* s = (struct rtp_sender_t*)param;
    assert(bytes <= sizeof(s->buffer));
    return s->buffer;
}

static void rtp_free(void* param, void *packet)
{
    struct rtp_sender_t* s = (struct rtp_sender_t*)param;
    assert(s->buffer == packet);
}

static int rtp_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
    struct rtp_sender_t* s = (struct rtp_sender_t*)param;
    assert(s->buffer == packet);
    
    int r = s->onpacket(s->param, packet, bytes, timestamp, flags);
    if(0 == r)
        rtp_onsend(s->rtp, packet, bytes/*, time*/);
    return r;
}

static void rtp_onrtcp(void* param, const struct rtcp_msg_t* msg)
{
    struct rtp_sender_t* s = (struct rtp_sender_t*)param;
    if(RTCP_MSG_BYE == msg->type && s->onbye)
        s->onbye(param);
}

int rtp_sender_init_video(struct rtp_sender_t* s, unsigned short port, int payload, const char* encoding, int frequence, const void* extra, size_t bytes)
{
    int r;
    struct rtp_event_t event;
    struct rtp_payload_t handler = {
        rtp_alloc,
        rtp_free,
        rtp_packet,
    };
    
    r = 0;
    memset(s, 0, sizeof(s));
    s->seq = (uint16_t)rtp_ssrc();
    s->ssrc = rtp_ssrc();
    s->timestamp = rtp_ssrc();
    s->frequency = 0 == frequence ? 90000 : frequence; // default 90MHz
    s->bandwidth = 2 * 1024 * 1024; // default 2Mb
    s->payload = payload;
    snprintf(s->encoding, sizeof(s->encoding)-1, "%s", encoding);

    if(payload >= 96)
    {
        if(0 == strcasecmp("H264", encoding) || 0 == strcasecmp("AVC", encoding))
        {
            r = sdp_h264(s->buffer, sizeof(s->buffer), port, payload, s->frequency, extra, bytes);
        }
        else if(0 == strcasecmp("H265", encoding) || 0 == strcasecmp("HEVC", encoding))
        {
            r = sdp_h265(s->buffer, sizeof(s->buffer), port, payload, s->frequency, extra, bytes);
        }
        else if(0 == strcasecmp("MP4V-ES", encoding))
        {
            r = sdp_mpeg4_es(s->buffer, sizeof(s->buffer), port, payload, s->frequency, extra, bytes);
        }
        else if (0 == strcasecmp(encoding, "MP2P"))
        {
            r = sdp_mpeg2_ps(s->buffer, sizeof(s->buffer), port, payload);
        }
        else if (0 == strcasecmp(encoding, "VP8"))
        {
            r = sdp_vp8(s->buffer, sizeof(s->buffer), port, payload);
        }
        else if (0 == strcasecmp(encoding, "VP9"))
        {
            r = sdp_vp9(s->buffer, sizeof(s->buffer), port, payload);
        }
        else
        {
            assert(0);
            return -1;
        }
    }
    else
    {
        switch(payload)
        {
        case RTP_PAYLOAD_MP2T:
            r = sdp_mpeg2_ts(s->buffer, sizeof(s->buffer), port);
            break;

        default:
            assert(0);
            return -1;
        }
    }
    
    s->encoder = rtp_payload_encode_create(payload, s->encoding, s->seq, s->ssrc, &handler, s);
    
    event.on_rtcp = rtp_onrtcp;
    s->rtp = rtp_create(&event, s, s->ssrc, s->timestamp, s->frequency, s->bandwidth, 1);

    if (r < 0 || r >= sizeof(s->buffer) || !s->rtp || !s->encoder)
    {
        rtp_sender_destroy(s);
        return -1;
    }
    return r;
}

int rtp_sender_init_audio(struct rtp_sender_t* s, unsigned short port, int payload, const char* encoding, int sample_rate, int channel_count, const void* extra, size_t bytes)
{
    int r;
    struct rtp_event_t event;
    struct rtp_payload_t handler = {
        rtp_alloc,
        rtp_free,
        rtp_packet,
    };
    
    r = 0;
    memset(s, 0, sizeof(s));
    s->seq = (uint16_t)rtp_ssrc();
    s->ssrc = rtp_ssrc();
    s->timestamp = rtp_ssrc();
    s->frequency = sample_rate;
    s->payload = payload;
    s->bandwidth = 128 * 1024; // default 128Kb
    snprintf(s->encoding, sizeof(s->encoding)-1, "%s", encoding);
    
    if(payload >= 96)
    {
        if(0 == strcasecmp("MP4A-LATM", encoding))
        {
            // RFC 6416
            s->bandwidth = 128 * 1024;
            r = sdp_aac_latm(s->buffer, sizeof(s->buffer), port, payload, sample_rate, channel_count, extra, bytes);
        }
        else if(0 == strcasecmp("MPEG4-GENERIC", encoding))
        {
            // RFC 3640 3.3.1. General (p21)
            s->bandwidth = 128 * 1024;
            r = sdp_aac_generic(s->buffer, sizeof(s->buffer), port, payload, sample_rate, channel_count, extra, bytes);
        }
        else if(0 == strcasecmp("opus", encoding))
        {
            // RFC7587 RTP Payload Format for the Opus Speech and Audio Codec
            s->bandwidth = 32000;
            r = sdp_opus(s->buffer, sizeof(s->buffer), port, payload, sample_rate, channel_count, extra, bytes);
        }
        else
        {
            assert(0);
            return -1;
        }
    }
    else
    {
        switch(payload)
        {
        case RTP_PAYLOAD_PCMU:
            s->bandwidth = 64000; // 8000 * 8 * 1
            snprintf(s->encoding, sizeof(s->encoding)-1, "%s", "PCMU");
            r = sdp_g711u(s->buffer, sizeof(s->buffer), port);
                
        case RTP_PAYLOAD_PCMA:
            s->bandwidth = 64000; // 8000 * 8 * 1
            snprintf(s->encoding, sizeof(s->encoding)-1, "%s", "PCMA");
            r = sdp_g711a(s->buffer, sizeof(s->buffer), port);
            break;
                
        default:
            assert(0);
            return -1;
        }
    }

    s->encoder = rtp_payload_encode_create(payload, s->encoding, s->seq, s->ssrc, &handler, s);

    event.on_rtcp = rtp_onrtcp;
    s->rtp = rtp_create(&event, s, s->ssrc, s->timestamp, s->frequency, s->bandwidth, 1);

    if (r < 0 || !s->rtp || !s->encoder)
    {
        rtp_sender_destroy(s);
        return -1;
    }
    return r;
}

int rtp_sender_destroy(struct rtp_sender_t* s)
{
    if (s->rtp)
    {
        rtp_destroy(s->rtp);
        s->rtp = NULL;
    }

    if (s->encoder)
    {
        rtp_payload_encode_destroy(s->encoder);
        s->encoder = NULL;
    }

    return 0;
}
