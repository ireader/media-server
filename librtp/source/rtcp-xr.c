#include "rtp-internal.h"
#include "rtp-util.h"
#include <errno.h>

// https://www.iana.org/assignments/rtcp-xr-block-types/rtcp-xr-block-types.xhtml
/*
    BT 	    Name 	                                        Reference
    1	    Loss RLE Report Block	                        [RFC3611]
    2	    Duplicate RLE Report Block	                    [RFC3611]
    3	    Packet Receipt Times Report Block	            [RFC3611]
    4	    Receiver Reference Time Report Block	        [RFC3611]
    5	    DLRR Report Block	                            [RFC3611]
    6	    Statistics Summary Report Block	                [RFC3611]
    7	    VoIP Metrics Report Block	                    [RFC3611]
    8	    RTCP XR	                                        [RFC5093]
    9	    Texas Instruments Extended VoIP Quality Block	[http://focus.ti.com/general/docs/bcg/bcgdoccenter.tsp?templateId=6116&navigationId=12078#42][David_Lide]
    10	    Post-repair Loss RLE Report Block	            [RFC5725]
    11	    Multicast Acquisition Report Block	            [RFC6332]
    12	    IDMS Report Block	                            [RFC7272]
    13	    ECN Summary Report	                            [RFC6679]
    14	    Measurement Information Block	                [RFC6776]
    15	    Packet Delay Variation Metrics Block	        [RFC6798]
    16	    Delay Metrics Block	                            [RFC6843]
    17	    Burst/Gap Loss Summary Statistics Block	        [RFC7004]
    18	    Burst/Gap Discard Summary Statistics Block	    [RFC7004]
    19	    Frame Impairment Statistics Summary	            [RFC7004]
    20	    Burst/Gap Loss Metrics Block	                [RFC6958]
    21	    Burst/Gap Discard Metrics Block	                [RFC7003][RFC Errata 3735]
    22	    MPEG2 Transport Stream PSI-Independent Decodability Statistics Metrics Block	[RFC6990]
    23	    De-Jitter Buffer Metrics Block	                [RFC7005]
    24	    Discard Count Metrics Block	                    [RFC7002]
    25	    DRLE (Discard RLE Report)	                    [RFC7097]
    26	    BDR (Bytes Discarded Report)	                [RFC7243]
    27	    RFISD (RTP Flows Initial Synchronization Delay)	[RFC7244]
    28	    RFSO (RTP Flows Synchronization Offset Metrics Block)	[RFC7244]
    29	    MOS Metrics Block	                            [RFC7266]
    30	    LCB (Loss Concealment Metrics Block)	        [RFC7294, Section 4.1]
    31	    CSB (Concealed Seconds Metrics Block)	        [RFC7294, Section 4.1]
    32	    MPEG2 Transport Stream PSI Decodability Statistics Metrics Block	[RFC7380]
    33	    Post-Repair Loss Count Metrics Report Block	    [RFC7509]
    34	    Video Loss Concealment Metric Report Block	    [RFC7867]
    35	    Independent Burst/Gap Discard Metrics Block	    [RFC8015]
    36-254  Unassigned
    255	    Reserved for future extensions	                [RFC3611]
*/

/*
Parameter 	Reference
pkt-loss-rle	[RFC3611]
pkt-dup-rle	[RFC3611]
pkt-rcpt-times	[RFC3611]
stat-summary	[RFC3611]
voip-metrics	[RFC3611]
rcvr-rtt	[RFC3611]
post-repair-loss-rle	[RFC5725]
grp-sync	[http://www.etsi.org/deliver/etsi_ts/183000_183099/183063/][ETSI 183 063][Miguel_Angel_Reina_Ortega]
multicast-acq	[RFC6332]
ecn-sum	[RFC6679]
pkt-dly-var	[RFC6798]
delay	[RFC6843]
burst-gap-loss-stat	[RFC7004]
burst-gap-discard-stat	[RFC7004]
frame-impairment-stat	[RFC7004]
burst-gap-loss	[RFC6958]
burst-gap-discard	[RFC7003]
ts-psi-indep-decodability	[RFC6990]
de-jitter-buffer	[RFC7005]
pkt-discard-count	[RFC7002]
discard-rle	[RFC7097]
discard-bytes	[RFC7243]
rtp-flow-init-syn-delay	[RFC7244]
rtp-flow-syn-offset	[RFC7244]
mos-metric	[RFC7266]
loss-conceal	[RFC7294]
conc-sec	[RFC7294]
ts-psi-decodability	[RFC7380]
post-repair-loss-count	[RFC7509]
video-loss-concealment	[RFC7867]
ind-burst-gap-discard	[RFC8015]
*/

static int rtcp_xr_rrt_pack(uint64_t ntp, uint8_t* ptr, uint32_t bytes);
static int rtcp_xr_dlrr_pack(const rtcp_dlrr_t* dlrr, int count, uint8_t* ptr, uint32_t bytes);
static int rtcp_xr_ecn_pack(const rtcp_ecn_t* ecn, uint8_t* ptr, uint32_t bytes);

static int rtcp_xr_lrle_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_xr_drle_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_xr_prt_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_xr_rrt_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_xr_dlrr_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);
static int rtcp_xr_ecn_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes);


// https://www.rfc-editor.org/rfc/rfc3611.html#section-4.1
/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=1      | rsvd. |   T   |         block length          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        SSRC of source                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          begin_seq            |             end_seq           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          chunk 1              |             chunk 2           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   :                              ...                              :
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          chunk n-1            |             chunk n           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_xr_lrle_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
    uint32_t i, j;
    uint32_t source;
    uint32_t len, num;
    uint16_t seq, end;
    uint32_t chunk;
    uint8_t* v, v0[32];

    len = nbo_r16(ptr + 2);

    if (bytes < 8 || (len + 1) * 4 > bytes)
        return -1;

    len *= 4; // to bytes
    if (len < 8)
        return 0;

    source = nbo_r32(ptr + 4);
    seq = nbo_r16(ptr + 8);
    end = nbo_r16(ptr + 10);
    num = end - seq;

    if ((num + 7) / 8 > sizeof(v0))
    {
        v = calloc((num + 7) / 8, sizeof(*v));
        if (!v) return -ENOMEM;
    }
    else
    {
        v = v0;
        memset(v, 0, (num + 7) / 8 * sizeof(v[0]));
    }

    ptr += 8;
    len -= 8;
    for(i = 0; len > 2; ptr += 2, len -= 2)
    {
        chunk = nbo_r16(ptr);
        if (0 == (0x8000 & chunk))
        {
            // Run Length Chunk
            for (j = 0; j < (chunk & 0x3FFF) && i < num; j++, i++)
            {
                if(0x4000 & chunk)
                    v[i/8] |= 1 << (7-(i%8));
            }
        }
        else
        {
            // Bit Vector Chunk
            for (j = 0; j < 15 && i < num; j++, i++)
            {
                if (chunk & (1 << (14 - j)))
                    v[i / 8] |= 1 << (7 - (i % 8));
            }
        }
    }

    msg->u.xr.u.rle.source = source;
    msg->u.xr.u.rle.begin = seq;
    msg->u.xr.u.rle.end = end;
    msg->u.xr.u.rle.chunk = v;
    msg->u.xr.u.rle.count = num;
    ctx->handler.on_rtcp(ctx->cbparam, msg);
    (void)ctx, (void)header;
    if (v && v != v0)
        free(v);
    return 0;
}

// https://www.rfc-editor.org/rfc/rfc3611.html#section-4.2
/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=2      | rsvd. |   T   |         block length          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        SSRC of source                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          begin_seq            |             end_seq           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          chunk 1              |             chunk 2           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   :                              ...                              :
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          chunk n-1            |             chunk n           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_xr_drle_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
    uint32_t i, j;
    uint32_t source;
    uint32_t len, num;
    uint16_t seq, end;
    uint32_t chunk;
    uint8_t* v, v0[32];

    len = nbo_r16(ptr + 2);

    if (bytes < 8 || (len + 1) * 4 > bytes)
        return -1;

    len *= 4; // to bytes
    if (len < 8)
        return 0;

    source = nbo_r32(ptr + 4);
    seq = nbo_r16(ptr + 8);
    end = nbo_r16(ptr + 10);
    num = end - seq;

    if ((num + 7) / 8 > sizeof(v0))
    {
        v = calloc((num + 7) / 8, sizeof(*v));
        if (!v) return -ENOMEM;
    }
    else
    {
        v = v0;
        memset(v, 0, (num + 7) / 8 * sizeof(v[0]));
    }

    ptr += 8;
    len -= 8;
    for (i = 0; len > 2; ptr += 2, len -= 2)
    {
        chunk = nbo_r16(ptr);
        if (0 == (0x8000 & chunk))
        {
            // Run Length Chunk
            for (j = 0; j < (chunk & 0x3FFF) && i < num; j++, i++)
            {
                if (0x4000 & chunk)
                    v[i / 8] |= 1 << (7 - (i % 8));
            }
        }
        else
        {
            // Bit Vector Chunk
            for (j = 0; j < 15 && i < num; j++, i++)
            {
                if (chunk & (1 << (14 - j)))
                    v[i / 8] |= 1 << (7 - (i % 8));
            }
        }
    }

    msg->u.xr.u.rle.source = source;
    msg->u.xr.u.rle.begin = seq;
    msg->u.xr.u.rle.end = end;
    msg->u.xr.u.rle.chunk = v;
    msg->u.xr.u.rle.count = num;
    ctx->handler.on_rtcp(ctx->cbparam, msg);
    (void)ctx, (void)header;
    if (v && v != v0)
        free(v);
    return 0;
}


// https://www.rfc-editor.org/rfc/rfc3611.html#section-4.3
/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=3      | rsvd. |   T   |         block length          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        SSRC of source                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          begin_seq            |             end_seq           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       Receipt time of packet begin_seq                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       Receipt time of packet (begin_seq + 1) mod 65536        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   :                              ...                              :
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       Receipt time of packet (end_seq - 1) mod 65536          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_xr_prt_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
    uint32_t i;
    uint32_t source;
    uint32_t len, num;
    uint16_t seq, end;
    uint32_t* timestamp, timestamp0[32];

    len = nbo_r16(ptr + 2);

    if (bytes < 8 || (len + 1) * 4 > bytes)
        return -1;

    len *= 4; // to bytes
    if (len < 8)
        return 0;

    source = nbo_r32(ptr + 4);
    seq = nbo_r16(ptr + 8);
    end = nbo_r16(ptr + 10);
    num = end - seq;

    if (num > sizeof(timestamp0)/sizeof(timestamp0[0]))
    {
        timestamp = calloc(num, sizeof(*timestamp));
        if (!timestamp) return -ENOMEM;
    }
    else
    {
        timestamp = timestamp0;
        memset(timestamp, 0, num * sizeof(timestamp[0]));
    }

    ptr += 8;
    len -= 8;
    for (i = 0; len > 4; ptr += 4, len -= 4)
    {
        timestamp[i] = nbo_r32(ptr);
    }

    msg->u.xr.u.prt.source = source;
    msg->u.xr.u.prt.begin = seq;
    msg->u.xr.u.prt.end = end;
    msg->u.xr.u.prt.timestamp = timestamp;
    msg->u.xr.u.prt.count = num;
    ctx->handler.on_rtcp(ctx->cbparam, msg);
    (void)ctx, (void)header;
    if (timestamp && timestamp != timestamp0)
        free(timestamp);
    return 0;
}


// https://www.rfc-editor.org/rfc/rfc3611.html#section-4.4
/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=4      |   reserved    |       block length = 2        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |              NTP timestamp, most significant word             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |             NTP timestamp, least significant word             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtcp_xr_rrt_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
    uint32_t len;
    uint64_t ntp;

    len = nbo_r16(ptr + 2);

    if (bytes < 12 || (len + 1) * 4 > bytes)
        return -1;

    ntp = nbo_r32(ptr + 4);
    ntp = (ntp << 32) | nbo_r32(ptr + 8);

    msg->u.xr.u.rrt = ntp;
    ctx->handler.on_rtcp(ctx->cbparam, msg);
    (void)ctx, (void)header;
    return 0;
}

static int rtcp_xr_rrt_pack(uint64_t ntp, uint8_t* ptr, uint32_t bytes)
{
    if (bytes < 12)
        return -1;

    nbo_w32(ptr, (RTCP_XR_RRT << 24) | 2);
    nbo_w32(ptr + 4, (uint32_t)(ntp >> 32));
    nbo_w32(ptr + 8, (uint32_t)ntp);
    return 12;
}

// https://www.rfc-editor.org/rfc/rfc3611.html#section-4.5
/*
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |     BT=5      |   reserved    |         block length          |
 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 |                 SSRC_1 (SSRC of first receiver)               | sub-
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
 |                         last RR (LRR)                         |   1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                   delay since last RR (DLRR)                  |
 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 |                 SSRC_2 (SSRC of second receiver)              | sub-
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
 :                               ...                             :   2
 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/
static int rtcp_xr_dlrr_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
    uint32_t i;
    uint32_t len, num;
    rtcp_dlrr_t* dlrr, dlrr0[32];

    len = nbo_r16(ptr + 2);
    if (bytes < 8 || (len + 1) * 4 > bytes)
        return -1;

    num = len / 3;
    if (num > sizeof(dlrr0))
    {
        dlrr = calloc(num, sizeof(*dlrr));
        if (!dlrr) return -ENOMEM;
    }
    else
    {
        dlrr = dlrr0;
        memset(dlrr, 0, num * sizeof(dlrr[0]));
    }

    ptr += 4;
    for (i = 0; i < num; i++, ptr += 12)
    {
        dlrr[i].ssrc = nbo_r32(ptr + 0);
        dlrr[i].lrr = nbo_r32(ptr + 4);
        dlrr[i].dlrr = nbo_r32(ptr + 8);
    }

    msg->u.xr.u.dlrr.dlrr = dlrr;
    msg->u.xr.u.dlrr.count = num;
    ctx->handler.on_rtcp(ctx->cbparam, msg);
    (void)ctx, (void)header;
    if (dlrr && dlrr != dlrr0)
        free(dlrr);
    return 0;
}

static int rtcp_xr_dlrr_pack(const rtcp_dlrr_t* dlrr, int count, uint8_t* ptr, uint32_t bytes)
{
    int i;
    if ((int)bytes < 4 + count * 12)
        return - 1;

    nbo_w32(ptr, (RTCP_XR_DLRR << 24) | (count * 3));
    bytes -= 4;
    ptr += 4;

    for (i = 0; i < count && bytes >= 12; i++)
    {
        nbo_w32(ptr, dlrr[i].ssrc);
        nbo_w32(ptr + 4, dlrr[i].lrr);
        nbo_w32(ptr + 8, dlrr[i].dlrr);

        bytes -= 12;
        ptr += 12;
    }
    return 4 + i * 12;
}

// https://www.rfc-editor.org/rfc/rfc7097.html#section-5
// rtcp-xr: discard-rle
/*
       0               1               2               3
       0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |     BT=25     |rsvd |E|   T   |         block length          |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                        SSRC of source                         |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |          begin_seq            |             end_seq           |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |          chunk 1              |             chunk 2           |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      :                              ...                              :
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |          chunk n-1            |             chunk n           |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/


// https://datatracker.ietf.org/doc/html/rfc7243#section-5
// rtcp-xr: discard-bytes
/*
    0               1               2               3
    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=26     | I |E|Reserved |       Block length=2          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        SSRC of source                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |             Number of RTP payload bytes discarded             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

// https://www.rfc-editor.org/rfc/rfc6679.html#section-5.2
/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=13     | Reserved      |         Block Length          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | SSRC of Media Sender                                          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | ECT (0) Counter                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | ECT (1) Counter                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | ECN-CE Counter                | not-ECT Counter               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | Lost Packets Counter          | Duplication Counter           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

                   Figure 4: RTCP XR ECN Summary Report
*/
static int rtcp_xr_ecn_unpack(struct rtp_context* ctx, const rtcp_header_t* header, struct rtcp_msg_t* msg, const uint8_t* ptr, size_t bytes)
{
    uint32_t len;
    rtcp_ecn_t ecn;
    
    len = nbo_r16(ptr + 2);
    if (bytes < 24 || (len + 1) * 4 > bytes)
        return -1;
    
    ecn.ext_highest_seq = nbo_r32(ptr); // ssrc
    ecn.ect[0] = nbo_r32(ptr + 4);
    ecn.ect[1] = nbo_r32(ptr + 8);
    ecn.ect_ce_counter = nbo_r16(ptr + 12);
    ecn.not_ect_counter = nbo_r16(ptr + 14);
    ecn.lost_packets_counter = nbo_r16(ptr + 16);
    ecn.duplication_counter = nbo_r16(ptr + 18);

    memcpy(&msg->u.xr.u.ecn, &ecn, sizeof(msg->u.xr.u.ecn));
    ctx->handler.on_rtcp(ctx->cbparam, msg);
    (void)ctx, (void)header;
    return 0;
}

static int rtcp_xr_ecn_pack(const rtcp_ecn_t* ecn, uint8_t* ptr, uint32_t bytes)
{
    if (bytes < 24)
        return -1;

    nbo_w32(ptr, (RTCP_XR_ECN << 24) | 5);
    nbo_w32(ptr + 4, ecn->ext_highest_seq);
    nbo_w32(ptr + 8, ecn->ect[0]);
    nbo_w32(ptr + 12, ecn->ect[1]);
    nbo_w16(ptr + 16, ecn->ect_ce_counter);
    nbo_w16(ptr + 18, ecn->not_ect_counter);
    nbo_w16(ptr + 20, ecn->lost_packets_counter);
    nbo_w16(ptr + 22, ecn->duplication_counter);
    return 24;
}

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|reserved |   PT=XR=207   |             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                              SSRC                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   :                         report blocks                         :
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      BT       | type-specific |         block length          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   :             type-specific block contents                      :
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
void rtcp_xr_unpack(struct rtp_context* ctx, const rtcp_header_t* header, const uint8_t* ptr, size_t bytes)
{
    int r;
    struct rtcp_msg_t msg;
    struct rtp_member* sender;

    if (bytes < 8 /*sizeof(rtcp_xr_t)*/)
    {
        assert(0);
        return;
    }

    msg.ssrc = nbo_r32(ptr);
    sender = rtp_sender_fetch(ctx, msg.ssrc);
    if (!sender) return; // error
    assert(sender != ctx->self);

    r = 0;
    ptr += 4;
    bytes -= 4;
    while (bytes >= 4)
    {
        msg.type = RTCP_XR | (ptr[0] << 8);

        switch (ptr[0])
        {
        case RTCP_XR_LRLE:
            r = rtcp_xr_lrle_unpack(ctx, header, &msg, ptr, bytes);
            break;

        case RTCP_XR_DRLE:
            r = rtcp_xr_drle_unpack(ctx, header, &msg, ptr, bytes);
            break;

        case RTCP_XR_PRT:
            r = rtcp_xr_prt_unpack(ctx, header, &msg, ptr, bytes);
            break;

        case RTCP_XR_RRT:
            r = rtcp_xr_rrt_unpack(ctx, header, &msg, ptr, bytes);
            break;

        case RTCP_XR_DLRR:
            r = rtcp_xr_dlrr_unpack(ctx, header, &msg, ptr, bytes);
            break;

        case RTCP_XR_ECN:
            r = rtcp_xr_ecn_unpack(ctx, header, &msg, ptr, bytes);
            break;

        default:
            //assert(0);
            r = 0; // ignore
            break;
        }
    }

    return;
}

int rtcp_xr_pack(struct rtp_context* ctx, uint8_t* data, int bytes, enum rtcp_xr_type_t id, const rtcp_xr_t* xr)
{
    int r;
    rtcp_header_t header;

    (void)ctx;
    if (bytes < 4 + 4)
        return 4 + 4;

    switch (id)
    {
    case RTCP_XR_RRT:
        r = rtcp_xr_rrt_pack(xr->u.rrt, data + 8, bytes - 8);
        break;

    case RTCP_XR_DLRR:
        r = rtcp_xr_dlrr_pack(xr->u.dlrr.dlrr, xr->u.dlrr.count, data + 8, bytes - 8);
        break;

    case RTCP_XR_ECN:
        r = rtcp_xr_ecn_pack(&xr->u.ecn, data + 8, bytes - 8);
        break;

    case RTCP_XR_LRLE:
    case RTCP_XR_DRLE:
    case RTCP_XR_PRT:
    default:
        assert(0);
        return -1;
    }

    header.v = 2;
    header.p = 0;
    header.pt = RTCP_XR;
    header.rc = id;
    header.length = (r + 4 + 3) / 4;
    nbo_write_rtcp_header(data, &header);

    nbo_w32(data + 4, ctx->self->ssrc);
    //nbo_w32(data + 4, xr->sender);

    //assert(8 == (header.length + 1) * 4);
    return header.length * 4 + 4;
}

#if defined(_DEBUG) || defined(DEBUG)
static void rtcp_on_xr_test(void* param, const struct rtcp_msg_t* msg)
{
    int r;
    static uint8_t buffer[1400];
    switch (msg->type & 0xFF)
    {
    case RTCP_XR:
        switch ((msg->type >> 8) & 0xFF)
        {
        case RTCP_XR_RRT:
            assert(0x1234567823456789 == msg->u.xr.u.rrt);
            r = rtcp_xr_rrt_pack(msg->u.xr.u.rrt, buffer, sizeof(buffer));
            assert(12 == r && 0 == memcmp(buffer, param, r));
            break;

        case RTCP_XR_DLRR:
            assert(1 == msg->u.xr.u.dlrr.count);
            assert(0x12345678 == msg->u.xr.u.dlrr.dlrr[0].ssrc && 0x23344556 == msg->u.xr.u.dlrr.dlrr[0].lrr && 0x33343536 == msg->u.xr.u.dlrr.dlrr[0].dlrr);
            r = rtcp_xr_dlrr_pack(msg->u.xr.u.dlrr.dlrr, msg->u.xr.u.dlrr.count, buffer, sizeof(buffer));
            assert(16 == r && 0 == memcmp(buffer, param, r));
            break;

        case RTCP_XR_ECN:
            r = rtcp_xr_ecn_pack(&msg->u.xr.u.ecn, buffer, sizeof(buffer));
            assert(r > 0 && 0 == memcmp(buffer, param, r));

        default:
            break;
        }
        break;

    default:
        assert(0);
    }
}

static void rtcp_rtpfb_rrt_test(void)
{
    const uint8_t data[] = { 0x04, 0x00, 0x00, 0x02, 0x12, 0x34, 0x56, 0x78, 0x23, 0x45, 0x67, 0x89 };

    struct rtcp_msg_t msg;
    struct rtp_context rtp;
    rtp.handler.on_rtcp = rtcp_on_xr_test;
    rtp.cbparam = (void*)data;

    msg.type = (RTCP_XR_RRT << 8) | RTCP_XR;
    assert(0 == rtcp_xr_rrt_unpack(&rtp, NULL, &msg, data, sizeof(data)));
}

static void rtcp_rtpfb_dlrr_test(void)
{
    const uint8_t data[] = { 0x05, 0x00, 0x00, 0x03, 0x12, 0x34, 0x56, 0x78, 0x23, 0x34, 0x45, 0x56, 0x33, 0x34, 0x35, 0x36 };

    struct rtcp_msg_t msg;
    struct rtp_context rtp;
    rtp.handler.on_rtcp = rtcp_on_xr_test;
    rtp.cbparam = (void*)data;

    msg.type = (RTCP_XR_DLRR << 8) | RTCP_XR;
    assert(0 == rtcp_xr_dlrr_unpack(&rtp, NULL, &msg, data, sizeof(data)));
}

static void rtcp_rtpfb_ecn_test(void)
{
    const uint8_t data[] = { 0x00 };

    struct rtcp_msg_t msg;
    struct rtp_context rtp;
    rtp.handler.on_rtcp = rtcp_on_xr_test;
    rtp.cbparam = (void*)data;

    msg.type = (RTCP_XR_ECN << 8) | RTCP_XR;
    assert(0 == rtcp_xr_ecn_unpack(&rtp, NULL, &msg, data, sizeof(data)));
}

void rtcp_xr_test(void)
{
    rtcp_rtpfb_rrt_test();
    rtcp_rtpfb_dlrr_test();
    //rtcp_rtpfb_ecn_test();
}
#endif
