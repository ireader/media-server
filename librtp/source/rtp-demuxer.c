#include "rtp-demuxer.h"
#include "rtp-internal.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include "rtcp-header.h"
#include "rtp-packet.h"
#include "rtp-queue.h"
#include "rtp.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

struct rtp_demuxer_t
{
    uint32_t ssrc;
    uint64_t clock; // rtcp clock
    
    uint8_t* ptr;
    int cap;

    rtp_queue_t* queue;
    void* payload;
    void* rtp;
    
    rtp_demuxer_onpacket onpkt;
    void* param;
};

static void rtp_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
    struct rtp_demuxer_t* rtp;
    rtp = (struct rtp_demuxer_t*)param;
    
    // TODO: rtp timestamp -> pts/dts
    
    if(rtp->onpkt)
        rtp->onpkt(rtp->param, packet, bytes, timestamp, flags);
}

static void rtp_on_rtcp(void* param, const struct rtcp_msg_t* msg)
{
    struct rtp_demuxer_t* rtp;
    rtp = (struct rtp_demuxer_t*)param;
    if (RTCP_MSG_BYE == msg->type)
    {
        printf("finished\n");
    }
}

static struct rtp_packet_t* rtp_demuxer_alloc(struct rtp_demuxer_t* rtp, const void* data, int bytes)
{
    int r;
    uint8_t* ptr;
    struct rtp_packet_t* pkt;
    
    if(rtp->cap < bytes + sizeof(struct rtp_packet_t))
    {
        ptr = (uint8_t*)realloc(rtp->ptr, bytes + sizeof(struct rtp_packet_t) + 1500 + sizeof(uint32_t));
        if(!ptr)
            return NULL;
        
        rtp->cap = bytes + sizeof(struct rtp_packet_t) + 1500;
        *(uint32_t*)ptr = rtp->cap;
        rtp->ptr = ptr + sizeof(uint32_t);
    }

    pkt = (struct rtp_packet_t*)rtp->ptr;
    memcpy(pkt + 1, data, bytes);
    
    r = rtp_packet_deserialize(pkt, pkt + 1, bytes);
    if(0 != r)
        return NULL;
    
    rtp->cap = 0; // need more memory
    rtp->ptr = NULL;
    return pkt;
}

static void rtp_demuxer_freepkt(void* param, struct rtp_packet_t* pkt)
{
    int cap;
    uint8_t* ptr;
    struct rtp_demuxer_t* rtp;
    rtp = (struct rtp_demuxer_t*)param;
    
    ptr = (uint8_t*)pkt - sizeof(int);
    cap = *(int*)ptr;
    
    if(cap <= rtp->cap)
    {
        free(ptr);
        return;
    }
    
    if(rtp->cap > 0 && rtp->ptr)
        free(rtp->ptr);
    rtp->cap = cap;
    rtp->ptr = (uint8_t*)pkt;
}

static int rtp_demuxer_init(struct rtp_demuxer_t* rtp, int frequency, int payload, const char* encoding)
{
    uint32_t timestamp;
    struct rtp_event_t evthandler;
    struct rtp_payload_t handler;
//    const struct rtp_profile_t* profile;
//    profile = rtp_profile_find(payload);
//    frequency = profile ? profile->frequency : 90000;

    memset(&handler, 0, sizeof(handler));
    handler.alloc = NULL;
    handler.free = NULL;
    handler.packet = rtp_packet;
    rtp->payload = rtp_payload_decode_create(payload, encoding, &handler, rtp);
    
    timestamp = (uint32_t)rtpclock();
    evthandler.on_rtcp = rtp_on_rtcp;
    rtp->rtp = rtp_create(&evthandler, rtp, rtp->ssrc, timestamp, frequency ? frequency : 90000, 2 * 1024 * 1024, 0);
    
    rtp->queue = rtp_queue_create(100, frequency, rtp_demuxer_freepkt, rtp->queue);
    
    return rtp->payload && rtp->rtp && rtp->queue? 0 : -1;
}

struct rtp_demuxer_t* rtp_demuxer_create(int frequency, int payload, const char* encoding, rtp_demuxer_onpacket onpkt, void* param)
{
    struct rtp_demuxer_t* rtp;
    rtp = (struct rtp_demuxer_t*)calloc(1, sizeof(*rtp));
    if(!rtp)
        return NULL;
    
    if(0 != rtp_demuxer_init(rtp, frequency, payload, encoding))
    {
        rtp_demuxer_destroy(&rtp);
        return NULL;
    }
    
    rtp->onpkt = onpkt;
    rtp->param = param;
    rtp->clock = rtpclock();
    rtp->ssrc = rtp_ssrc();
    return rtp;
}

int rtp_demuxer_destroy(struct rtp_demuxer_t** pprtp)
{
    struct rtp_demuxer_t* rtp;
    if(pprtp && *pprtp)
    {
        rtp = *pprtp;
        if(rtp->rtp)
            rtp_destroy(rtp->rtp);
        
        if(rtp->payload)
            rtp_payload_decode_destroy(rtp->payload);
        
        if(rtp->queue)
            rtp_queue_destroy(rtp->queue);
        
        if(rtp->ptr)
            free(rtp->ptr);
        free(rtp);
    }
    
    return 0;
}

int rtp_demuxer_input(struct rtp_demuxer_t* rtp, const void* data, int bytes)
{
    int r;
    struct rtp_packet_t* pkt;
    
    pkt = rtp_demuxer_alloc(rtp, data, bytes);
    if(!pkt)
        return -1;
    
    if(pkt->rtp.pt < RTCP_FIR || pkt->rtp.pt > RTCP_TOKEN)
    {
        r = rtp_queue_write(rtp->queue, pkt);
        if(0 != r)
        {
            rtp_demuxer_freepkt(rtp, pkt);
            return r > 0 ? -r : r;
        }
        
        // re-order packet
        rtp_demuxer_freepkt(rtp, pkt);
        while(pkt)
        {
            bytes = (int)((uint8_t*)pkt->payload - (uint8_t*)(pkt+1)) + pkt->payloadlen;
            
            r = rtp_onreceived(rtp->rtp, pkt + 1, bytes);
            r = rtp_payload_decode_input(rtp->payload, pkt + 1, bytes);
            if(0 != r)
            {
                r = r > 0 ? -r : r;
                break;
            }
    
            rtp_demuxer_freepkt(rtp, pkt);
            pkt = rtp_queue_read(rtp->queue);
        }
    }
    else
    {
        r = rtp_onreceived_rtcp(rtp->rtp, pkt + 1, bytes);
        r = pkt->rtp.pt; // rtcp message type
        rtp_demuxer_freepkt(rtp, pkt);
    }
    
    return r;
}

int rtp_demuxer_rtcp(struct rtp_demuxer_t* rtp, void* buf, int len)
{
    int r;
    int interval;
    uint64_t clock;
    
    r = 0;
    clock = rtpclock();
    interval = rtp_rtcp_interval(rtp->rtp);
    if (rtp->clock + interval < clock)
    {
        // RTCP report
        r = rtp_rtcp_report(rtp->rtp, buf, len);
        rtp->clock = clock;
    }
    
    return r;
}
