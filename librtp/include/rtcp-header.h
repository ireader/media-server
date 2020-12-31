#ifndef _rtcp_header_h_ 
#define _rtcp_header_h_

#include <stdint.h>

enum
{
    RTCP_FIR    = 192,
    RTCP_NACK   = 193,
    RTCP_SMPTETC= 194,
    RTCP_IJ     = 195,
    
	RTCP_SR		= 200,
	RTCP_RR		= 201,
	RTCP_SDES	= 202,
	RTCP_BYE	= 203,
	RTCP_APP	= 204,
    
    RTCP_RTPFB  = 205,
    RTCP_PSFB   = 206,
    RTCP_XR     = 207,
    RTCP_AVB    = 208,
    RTCP_RSI    = 209,
    RTCP_TOKEN  = 210,
};

enum
{
	RTCP_SDES_END		= 0,
	RTCP_SDES_CNAME		= 1,
	RTCP_SDES_NAME		= 2,
	RTCP_SDES_EMAIL		= 3,
	RTCP_SDES_PHONE		= 4,
	RTCP_SDES_LOC		= 5,
	RTCP_SDES_TOOL		= 6,
	RTCP_SDES_NOTE		= 7,
	RTCP_SDES_PRIVATE	= 8,
};

typedef struct _rtcp_header_t
{
	uint32_t v:2;		// version
	uint32_t p:1;		// padding
	uint32_t rc:5;		// reception report count
	uint32_t pt:8;		// packet type
	uint32_t length:16; /* pkt len in words, w/o this word */
} rtcp_header_t;

typedef struct _rtcp_sr_t // sender report
{
	uint32_t ssrc;
	uint32_t ntpmsw; // ntp timestamp MSW(in second)
	uint32_t ntplsw; // ntp timestamp LSW(in picosecond)
	uint32_t rtpts;	 // rtp timestamp
	uint32_t spc;	 // sender packet count
	uint32_t soc;	 // sender octet count
} rtcp_sr_t;

typedef struct _rtcp_rr_t // receiver report
{
	uint32_t ssrc;
} rtcp_rr_t;

typedef struct _rtcp_rb_t // report block
{
	uint32_t ssrc;
	uint32_t fraction:8; // fraction lost
	uint32_t cumulative:24; // cumulative number of packets lost
	uint32_t exthsn; // extended highest sequence number received
	uint32_t jitter; // interarrival jitter
	uint32_t lsr; // last SR
	uint32_t dlsr; // delay since last SR
} rtcp_rb_t;

typedef struct _rtcp_sdes_item_t // source description RTCP packet
{
	uint8_t pt; // chunk type
	uint8_t len;
	uint8_t *data;
} rtcp_sdes_item_t;

#define RTCP_V(v)	((v >> 30) & 0x03) // rtcp version
#define RTCP_P(v)	((v >> 29) & 0x01) // rtcp padding
#define RTCP_RC(v)	((v >> 24) & 0x1F) // rtcp reception report count
#define RTCP_PT(v)	((v >> 16) & 0xFF) // rtcp packet type
#define RTCP_LEN(v)	(v & 0xFFFF) // rtcp packet length

#endif /* !_rtcp_header_h_ */
