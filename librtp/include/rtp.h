#ifndef _rtp_h_
#define _rtp_h_

typedef struct _rtp_header_t
{
	unsigned int v:2;		/* protocol version */
	unsigned int p:1;		/* padding flag */
	unsigned int x:1;		/* header extension flag */
	unsigned int cc:4;		/* CSRC count */
	unsigned int m:1;		/* marker bit */
	unsigned int pt:7;		/* payload type */
	unsigned int seq:16;	/* sequence number */
	unsigned int timestamp; /* timestamp */
	unsigned int ssrc;		/* synchronization source */
} rtp_header_t;

#define RTP_V(v)	((v >> 30) & 0x03) /* protocol version */
#define RTP_P(v)	((v >> 29) & 0x01) /* padding flag */
#define RTP_X(v)	((v >> 28) & 0x01) /* header extension flag */
#define RTP_CC(v)	((v >> 24) & 0x0F) /* CSRC count */
#define RTP_M(v)	((v >> 23) & 0x01) /* marker bit */
#define RTP_PT(v)	((v >> 16) & 0x7F) /* payload type */
#define RTP_SEQ(v)	((v >> 00) & 0xFFFF) /* sequence number */

#endif /* !_rtp_h_ */
