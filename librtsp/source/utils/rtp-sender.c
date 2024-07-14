#include "rtp-sender.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include "sdp-payload.h"
#include "rtsp-payloads.h"
#include "rtp.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#if defined(OS_WINDOWS)
    #if !defined(strcasecmp)
        #define strcasecmp	_stricmp
    #endif
#endif

uint32_t rtp_ssrc(void);

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
    if(RTCP_BYE == msg->type && s->onbye)
        s->onbye(param);
}

int rtp_sender_init_video(struct rtp_sender_t* s, const char* proto, unsigned short port, int payload, const char* encoding, int frequence, const void* extra, size_t bytes)
{
    int avp, r;
    struct rtp_event_t event;
    struct rtp_payload_t handler = {
        rtp_alloc,
        rtp_free,
        rtp_packet,
    };
    
    //memset(s, 0, sizeof(*s));
    s->seq = s->seq ? s->seq : (uint16_t)rtp_ssrc();
    s->ssrc = s->ssrc ? s->ssrc : rtp_ssrc();
    s->timestamp = s->timestamp ? s->timestamp : rtp_ssrc();
    s->bandwidth = s->bandwidth ? s->bandwidth : 2 * 1024 * 1024; // default 2Mb
    s->frequency = 0 == frequence ? 90000 : frequence; // default 90MHz
    s->payload = payload;
    snprintf(s->encoding, sizeof(s->encoding)-1, "%s", encoding);

    avp = avpayload_find_by_rtp((uint8_t)payload, encoding);
    if (avp < 0)
    {
        assert(0);
        return -EPROTONOSUPPORT;
    }

    r = sdp_payload_video(s->buffer, sizeof(s->buffer), s_payloads[avp].payload, proto, port, payload, s->frequency, extra, (int)bytes);
    if (r < 0)
    {
        assert(0);
        return -EPROTONOSUPPORT;
    }
    
    s->encoder = rtp_payload_encode_create(payload, s->encoding, s->seq, s->ssrc, &handler, s);
    
    event.on_rtcp = rtp_onrtcp;
    s->rtp = rtp_create(&event, s, s->ssrc, s->timestamp, s->frequency, s->bandwidth, 1);

    if (r < 0 || r >= sizeof(s->buffer) || !s->rtp || !s->encoder)
    {
        rtp_sender_destroy(s);
        return -ENOMEM;
    }
    return r;
}

int rtp_sender_init_audio(struct rtp_sender_t* s, const char* proto, unsigned short port, int payload, const char* encoding, int sample_rate, int channel_count, const void* extra, size_t bytes)
{
    int avp, r;
    struct rtp_event_t event;
    struct rtp_payload_t handler = {
        rtp_alloc,
        rtp_free,
        rtp_packet,
    };
    
    r = 0;
//  memset(s, 0, sizeof(*s));
    s->seq = s->seq ? s->seq : (uint16_t)rtp_ssrc();
    s->ssrc = s->ssrc ? s->ssrc : rtp_ssrc();
    s->timestamp = s->timestamp ? s->timestamp : rtp_ssrc();
    s->bandwidth = s->bandwidth ? s->bandwidth : 128 * 1024; // default 128Kb
    s->frequency = sample_rate;
    s->payload = payload;
    snprintf(s->encoding, sizeof(s->encoding)-1, "%s", encoding);
    
    avp = avpayload_find_by_rtp((uint8_t)payload, encoding);
    if (avp < 0)
    {
        assert(0);
        return -EPROTONOSUPPORT;
    }

    r = sdp_payload_audio(s->buffer, sizeof(s->buffer), s_payloads[avp].payload, proto, port, payload, sample_rate, channel_count, extra, (int)bytes);
    if (r < 0)
    {
        assert(0);
        return -EPROTONOSUPPORT;
    }

    switch(s_payloads[avp].payload)
    {
    case RTP_PAYLOAD_MP4A: // RFC 3640 3.3.1. General (p21)
    case RTP_PAYLOAD_LATM: // RFC 6416
        s->bandwidth = 128 * 1024;
        break;

    case RTP_PAYLOAD_OPUS: // RFC7587 RTP Payload Format for the Opus Speech and Audio Codec
        s->bandwidth = 32000;
        break;

    case RTP_PAYLOAD_PCMU:
        s->bandwidth = 64000; // 8000 * 8 * 1
        break;
                
    case RTP_PAYLOAD_PCMA:
        s->bandwidth = 64000; // 8000 * 8 * 1
        break;
                
    default:
        s->bandwidth = 128 * 1024; // default 128Kb
    }

    s->encoder = rtp_payload_encode_create(payload, s->encoding, s->seq, s->ssrc, &handler, s);

    event.on_rtcp = rtp_onrtcp;
    s->rtp = rtp_create(&event, s, s->ssrc, s->timestamp, s->frequency, s->bandwidth, 1);

    if (r < 0 || !s->rtp || !s->encoder)
    {
        rtp_sender_destroy(s);
        return -ENOMEM;
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
