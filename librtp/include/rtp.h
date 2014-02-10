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

#endif /* !_rtp_h_ */
