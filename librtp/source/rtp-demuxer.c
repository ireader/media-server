#include "rtp-demuxer.h"
#include "rtp-internal.h"
#include "rtp-payload.h"
#include "rtp-packet.h"
#include "rtp-queue.h"
#include "rtp-param.h"
#include "rtp.h"
#include "rtcp-header.h"
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
    int cap, max;

    rtp_queue_t* queue;
    void* payload;
    void* rtp;
    
    rtp_demuxer_onpacket onpkt;
    void* param;
};

static int rtp_onpacket(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
    struct rtp_demuxer_t* rtp;
    rtp = (struct rtp_demuxer_t*)param;
    
    // TODO: rtp timestamp -> pts/dts
    
    return rtp->onpkt ? rtp->onpkt(rtp->param, packet, bytes, timestamp, flags) : -1;
}

static void rtp_on_rtcp(void* param, const struct rtcp_msg_t* msg)
{
    //struct rtp_demuxer_t* rtp;
    //rtp = (struct rtp_demuxer_t*)param;
    if (RTCP_BYE == msg->type)
    {
        printf("finished: %p\n", param);
        //rtp->onpkt(rtp->param, NULL, 0, 0, 0);
    }
}

static struct rtp_packet_t* rtp_demuxer_alloc(struct rtp_demuxer_t* rtp, const void* data, int bytes)
{
    int r;
    uint8_t* ptr;
    struct rtp_packet_t* pkt;
    
    if(rtp->cap < bytes + (int)sizeof(struct rtp_packet_t) + (int)sizeof(int) /*bytes*/ )
    {
        r = bytes + sizeof(struct rtp_packet_t) + sizeof(int);
        r = r > 1500 ? r : 1500;
        ptr = (uint8_t*)realloc(rtp->ptr, r + sizeof(int) /*cap*/ );
        if(!ptr)
            return NULL;
        
        rtp->cap = r;
        rtp->ptr = ptr;
        *(int*)ptr = r; /*cap*/
    }

    *((int*)rtp->ptr + 1) = bytes; /*bytes*/
    pkt = (struct rtp_packet_t*)(rtp->ptr + sizeof(int) /*cap*/  + sizeof(int) /*bytes*/ );
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
    ptr = (uint8_t*)pkt - sizeof(int) /*cap*/ - sizeof(int) /*bytes*/ ;
    cap = *(int*)ptr;
    
    if(cap <= rtp->cap)
    {
        free(ptr);
        return;
    }
    
    if(rtp->cap > 0 && rtp->ptr)
        free(rtp->ptr);
    rtp->cap = cap;
    rtp->ptr = ptr;
}

static int rtp_demuxer_init(struct rtp_demuxer_t* rtp, int jitter, int frequency, int payload, const char* encoding)
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
    handler.packet = rtp_onpacket;
    rtp->payload = rtp_payload_decode_create(payload, encoding, &handler, rtp);
    
    timestamp = (uint32_t)rtpclock();
    evthandler.on_rtcp = rtp_on_rtcp;
    rtp->rtp = rtp_create(&evthandler, rtp, rtp->ssrc, timestamp, frequency ? frequency : 90000, 2 * 1024 * 1024, 0);
    
    rtp->queue = rtp_queue_create(jitter, frequency, rtp_demuxer_freepkt, rtp);
    
    return rtp->payload && rtp->rtp && rtp->queue? 0 : -1;
}

struct rtp_demuxer_t* rtp_demuxer_create(int jitter, int frequency, int payload, const char* encoding, rtp_demuxer_onpacket onpkt, void* param)
{
    struct rtp_demuxer_t* rtp;
    rtp = (struct rtp_demuxer_t*)calloc(1, sizeof(*rtp));
    if(!rtp)
        return NULL;
    
    if(0 != rtp_demuxer_init(rtp, jitter, frequency, payload, encoding))
    {
        rtp_demuxer_destroy(&rtp);
        return NULL;
    }
    
    rtp->onpkt = onpkt;
    rtp->param = param;
    rtp->clock = rtpclock();
    rtp->ssrc = rtp_ssrc();
    rtp->max = RTP_PAYLOAD_MAX_SIZE;
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
    uint8_t pt;
    struct rtp_packet_t* pkt;
    
    if (bytes < 12 || bytes > rtp->max)
        return -EINVAL;

    pt = ((uint8_t*)data)[1];
    // RFC7983 SRTP: https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
    // http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml#rtp-parameters-4
    // RFC 5761 (RTCP-mux) states this range for secure RTCP/RTP detection.
    // RTCP packet types in the ranges 1-191 and 224-254 SHOULD only be used when other values have been exhausted.
    if(pt < RTCP_FIR || pt > RTCP_LIMIT)
    {
        pkt = rtp_demuxer_alloc(rtp, data, bytes);
        if (!pkt)
            return -ENOMEM;

        r = rtp_queue_write(rtp->queue, pkt);
        if(r <= 0) // 0-discard packet(duplicate/too late)
        {
            rtp_demuxer_freepkt(rtp, pkt);
            return r;
        }
        
        // re-order packet
        pkt = rtp_queue_read(rtp->queue);
        while(pkt)
        {
            bytes = *(int*)((uint8_t*)pkt - sizeof(int) /*bytes*/ );

            r = rtp_onreceived(rtp->rtp, pkt + 1, bytes);
            r = rtp_payload_decode_input(rtp->payload, pkt + 1, bytes);
            rtp_demuxer_freepkt(rtp, pkt);
            if(r < 0)
                return r;
    
            pkt = rtp_queue_read(rtp->queue);
        }
    }
    else
    {
        r = rtp_onreceived_rtcp(rtp->rtp, data, bytes);
        (void)r; // ignore rtcp handler
        
        return pt; // rtcp message type
    }

    return 0;
}

int rtp_demuxer_rtcp(struct rtp_demuxer_t* rtp, void* buf, int len)
{
    int r;
    int interval;
    uint64_t clock;
    
    r = 0;
    clock = rtpclock();
    interval = rtp_rtcp_interval(rtp->rtp);
    if (rtp->clock + (uint64_t)interval * 1000 < clock)
    {
        // RTCP report
        r = rtp_rtcp_report(rtp->rtp, buf, len);
        rtp->clock = clock;
    }
    
    return r;
}

void rtp_demuxer_stats(struct rtp_demuxer_t* rtp, int* lost, int* late, int* misorder, int* duplicate)
{
    struct rtp_queue_stats_t stats;
    rtp_queue_stats(rtp->queue, &stats);
    if (lost) *lost = stats.lost;
    if (late) *late = stats.late;
    if (misorder) *misorder = stats.reorder;
    if (duplicate) *duplicate = stats.duplicate;
}
