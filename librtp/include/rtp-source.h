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

	unsigned int ssrc;

	char *cname;
	char *name;
	char *email;
	char *phone;
	char *loc;
	char *tool;
	char *note;

	time64_t rtp_clock;				// last RTP packet received time
	unsigned int rtp_timestamp;		// last RTP packet timestamp(in packet header)

	time64_t rtcp_clock;			// last RTCP packet received time
	unsigned int rtcp_ntp_msw;		// last RTCP packet NTP timestamp(in second)
	unsigned int rtcp_ntp_lsw;		// last RTCP packet NTP timestamp(in picosecond)
	unsigned int rtcp_rtp_timestamp;// last RTCP packet RTP timestamp(in packet header)
	unsigned int rtcp_spc;			// last RTCP packet packet count
	unsigned int rtcp_soc;			// last RTCP packet byte count

	double jitter;

	unsigned short seq_base;		// max sequence number
	unsigned short seq_max;			// max sequence number
	unsigned short seq_cycles;		// high extension sequence number

	unsigned int expected_prior;	// packet count
	unsigned int packets_prior;		// packet count
	unsigned int packets_recevied;	// received packet count
	unsigned int bytes_received;
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
