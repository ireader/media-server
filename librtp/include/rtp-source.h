#ifndef _rtp_source_h_
#define _rtp_source_h_

#if defined(OS_WINDOWS)
	typedef __int64 int64_t;
#else
	typedef long long int64_t;
#endif

typedef int64_t time64_t;

struct rtp_source
{
	long ref;
	unsigned short seq;				// max sequence number
	unsigned short extseq;			// high extension sequence number
	unsigned int packets;			// 2nd packet count
	unsigned int packets_expected;	// packet count
	unsigned int packets_recevied;	// received packet count
	unsigned int bytes;
	unsigned int bytes_expected;
	unsigned int bytes_received;

	unsigned int rtp_timestamp;

	unsigned int ssrc;

	char *cname;
	char *name;
	char *email;
	char *phone;
	char *loc;
	char *tool;
	char *note;

	// sender only
	time64_t ntpts;		// NTP timestamp
	unsigned int rtpts;	// RTP timestamp
	unsigned int spc;	// sender packet count
	unsigned int soc;	// sender octet count
};

struct rtp_source* rtp_source_create(unsigned int ssrc);
void rtp_source_release(struct rtp_source *source);

void rtp_source_setcname(struct rtp_source *source, const char* data, int bytes);
void rtp_source_setname(struct rtp_source *source, const char* data, int bytes);
void rtp_source_setemail(struct rtp_source *source, const char* data, int bytes);
void rtp_source_setphone(struct rtp_source *source, const char* data, int bytes);
void rtp_source_setloc(struct rtp_source *source, const char* data, int bytes);
void rtp_source_settool(struct rtp_source *source, const char* data, int bytes);
void rtp_source_setnote(struct rtp_source *source, const char* data, int bytes);

#endif /* !_rtp_source_h_ */
