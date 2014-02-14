#ifndef _rtcp_h_
#define _rtcp_h_

enum
{
	RTCP_SR		= 200,
	RTCP_RR		= 201,
	RTCP_SDES	= 202,
	RTCP_BYE	= 203,
	RTCP_APP	= 204,
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
	unsigned int v:2;		// version
	unsigned int p:1;		// padding
	unsigned int rc:5;		// reception report count
	unsigned int pt:8;		// packet type
	unsigned int length:16; /* pkt len in words, w/o this word */
} rtcp_header_t;

typedef struct _rtcp_sr_t // sender report
{
	unsigned int ssrc;
	unsigned int ntpts0; // ntp timestamp MSW
	unsigned int ntpts1; // ntp timestamp LSW
	unsigned int rtpts; // rtp timestamp
	unsigned int spc; // sender packet count
	unsigned int soc; // sender octet count
} rtcp_sr_t;

typedef struct _rtcp_rr_t // receiver report
{
	unsigned int ssrc;
} rtcp_rr_t;

typedef struct _rtcp_rb_t // report block
{
	unsigned int ssrc;
	unsigned int fraction:8; // fraction lost
	unsigned int cumulative:24; // cumulative number of packets lost
	unsigned int exthsn; // extended highest sequence number received
	unsigned int jitter; // interarrival jitter
	unsigned int lsr; // last SR
	unsigned int dlsr; // delay since last SR
} rtcp_rb_t;

typedef struct _rtcp_sdes_item_t // source description RTCP packet
{
	unsigned char pt; // chunk type
	unsigned char len;
	char data[1];
} rtcp_sdes_item_t;

#define RTCP_V(v)	((v >> 30) & 0x03) // rtcp version
#define RTCP_P(v)	((v >> 29) & 0x01) // rtcp padding
#define RTCP_RC(v)	((v >> 24) & 0x1F) // rtcp reception report count
#define RTCP_PT(v)	((v >> 16) & 0xFF) // rtcp packet type
#define RTCP_LEN(v)	(v & 0xFFFF) // rtcp packet length

#endif /* !_rtcp_h_ */
