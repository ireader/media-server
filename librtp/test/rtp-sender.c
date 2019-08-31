#include "rtp-sender.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include "rtp.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

uint32_t rtp_ssrc(void);
int sdp_h264(uint8_t *data, int bytes, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
int sdp_h265(uint8_t *data, int bytes, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
int sdp_mpeg4_es(uint8_t *data, int bytes, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
int sdp_opus(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
int sdp_aac_latm(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
int sdp_aac_generic(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
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

static void rtp_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
    struct rtp_sender_t* s = (struct rtp_sender_t*)param;
    assert(s->buffer == packet);
    
    int r = s->send(s->param, packet, bytes);
    if(r == bytes)
        rtp_onsend(s->rtp, packet, bytes/*, time*/);
}

static void rtp_onrtcp(void* param, const struct rtcp_msg_t* msg)
{
    struct rtp_sender_t* s = (struct rtp_sender_t*)param;
    if(RTCP_MSG_BYE == msg->type && s->onbye)
        s->onbye(param);
}

int rtp_sender_init_video(struct rtp_sender_t* s, int port, int payload, const char* encoding, int width, int height, const void* extra, size_t bytes)
{
    struct rtp_event_t event;
    struct rtp_payload_t handler = {
        rtp_alloc,
        rtp_free,
        rtp_packet,
    };
    
    s->seq = (uint16_t)rtp_ssrc();
    s->ssrc = rtp_ssrc();
    s->timestamp = rtp_ssrc();
    s->frequency = 90000;
    s->bandwidth = 1000000;
    s->payload = payload;
    snprintf(s->encoding, sizeof(s->encoding)-1, "%s", encoding);

    if(payload >= 96)
    {
        if(0 == strcmp("H264", encoding))
        {
            s->encoder = rtp_payload_encode_create(payload, s->encoding, s->seq, s->ssrc, &handler, s);
            sdp_h264(s->buffer, sizeof(s->buffer), port, payload, s->frequency, extra, bytes);
        }
        else if(0 == strcmp("H265", encoding))
        {
            s->encoder = rtp_payload_encode_create(payload, s->encoding, s->seq, s->ssrc, &handler, s);
            sdp_h265(s->buffer, sizeof(s->buffer), port, payload, s->frequency, extra, bytes);
        }
        else if(0 == strcmp("MP4V-ES", encoding))
        {
            s->encoder = rtp_payload_encode_create(payload, s->encoding, s->seq, s->ssrc, &handler, s);
            sdp_mpeg4_es(s->buffer, sizeof(s->buffer), port, payload, s->frequency, extra, bytes);
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
            default:
                assert(0);
                return -1;
        }
    }
    
    event.on_rtcp = rtp_onrtcp;
    s->rtp = rtp_create(&event, s, s->ssrc, s->timestamp, s->frequency, s->bandwidth, 1);
    return 0;
}

int rtp_sender_init_audio(struct rtp_sender_t* s, int port, int payload, const char* encoding, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes)
{
    struct rtp_event_t event;
    struct rtp_payload_t handler = {
        rtp_alloc,
        rtp_free,
        rtp_packet,
    };
    
    s->seq = (uint16_t)rtp_ssrc();
    s->ssrc = rtp_ssrc();
    s->timestamp = rtp_ssrc();
    s->frequency = sample_rate;
    s->payload = payload;
    snprintf(s->encoding, sizeof(s->encoding)-1, "%s", encoding);
    
    if(payload >= 96)
    {
        if(0 == strcmp("MP4A-LATM", encoding))
        {
            // RFC 6416
            s->bandwidth = 128 * 1024;
            s->encoder = rtp_payload_encode_create(payload, s->encoding, s->seq, s->ssrc, &handler, s);
            sdp_aac_latm(s->buffer, sizeof(s->buffer), port, payload, sample_rate, channel_count, extra, bytes);
        }
        else if(0 == strcmp("MPEG4-GENERIC", encoding))
        {
            // RFC 3640 3.3.1. General (p21)
            s->bandwidth = 128 * 1024;
            s->encoder = rtp_payload_encode_create(payload, s->encoding, s->seq, s->ssrc, &handler, s);
            sdp_aac_generic(s->buffer, sizeof(s->buffer), port, payload, sample_rate, channel_count, extra, bytes);
        }
        else if(0 == strcmp("opus", encoding))
        {
            // RFC7587 RTP Payload Format for the Opus Speech and Audio Codec
            s->bandwidth = 32000;
            s->encoder = rtp_payload_encode_create(payload, s->encoding, s->seq, s->ssrc, &handler, s);
            sdp_opus(s->buffer, sizeof(s->buffer), port, payload, sample_rate, channel_count, extra, bytes);
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
                s->encoder = rtp_payload_encode_create(payload, s->encoding, s->seq, s->ssrc, &handler, s);
                sdp_g711u(s->buffer, sizeof(s->buffer), port);
                
            case RTP_PAYLOAD_PCMA:
                s->bandwidth = 64000; // 8000 * 8 * 1
                snprintf(s->encoding, sizeof(s->encoding)-1, "%s", "PCMA");
                s->encoder = rtp_payload_encode_create(payload, s->encoding, s->seq, s->ssrc, &handler, s);
                sdp_g711a(s->buffer, sizeof(s->buffer), port);
                break;
                
            default:
                assert(0);
                return -1;
        }
    }
    
    event.on_rtcp = rtp_onrtcp;
    s->rtp = rtp_create(&event, s, s->ssrc, s->timestamp, s->frequency, s->bandwidth, 1);
    return 0;
}
