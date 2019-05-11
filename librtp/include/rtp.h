#ifndef _rtp_h_
#define _rtp_h_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { 
	RTCP_MSG_MEMBER,	/// new member(re-calculate RTCP Transmission Interval)
	RTCP_MSG_EXPIRED,	/// member leave(re-calculate RTCP Transmission Interval)
	RTCP_MSG_BYE,		/// member leave(re-calculate RTCP Transmission Interval)
	RTCP_MSG_APP, 
};

struct rtcp_msg_t
{
	int type;
	union rtcp_msg_u
	{
		// RTCP_MSG_MEMBER
		struct rtcp_member_t
		{
			unsigned int ssrc;
		} member;

		// RTCP_MSG_EXPIRED
		struct rtcp_expired_t
		{
			unsigned int ssrc;
		} expired;

		// RTCP_MSG_BYE
		struct rtcp_bye_t
		{
			unsigned int ssrc;
			const void* reason;
			int bytes; // reason length
		} bye;

		// RTCP_MSG_APP
		struct rtcp_app_t
		{
			unsigned int ssrc;
			char name[4];
			void* data;
			int bytes; // data length
		} app;
	} u;
};

struct rtp_event_t
{
	void (*on_rtcp)(void* param, const struct rtcp_msg_t* msg);
};

/// @param[in] ssrc RTP SSRC
/// @param[in] timestamp base timestamp
/// @param[in] frequence RTP frequence
/// @param[in] bandwidth in byte
/// @param[in] sender 1-rtp sender(SR), 0-rtp receiver(RR)
void* rtp_create(struct rtp_event_t *handler, void* param, uint32_t ssrc, uint32_t timestamp, int frequence, int bandwidth, int sender);
int rtp_destroy(void* rtp);

/// RTP send notify
/// @param[in] rtp RTP object
/// @param[in] data RTP packet(include RTP Header)
/// @param[in] bytes RTP packet size in byte
/// @return 0-ok, <0-error
int rtp_onsend(void* rtp, const void* data, int bytes);

/// RTP receive notify
/// @param[in] rtp RTP object
/// @param[in] data RTP packet(include RTP Header)
/// @param[in] bytes RTP packet size in byte
/// @return 1-ok, 0-rtp packet ok, seq disorder, <0-error
int rtp_onreceived(void* rtp, const void* data, int bytes);

/// received RTCP packet
/// @param[in] rtp RTP object
/// @param[in] rtcp RTCP packet(include RTCP Header)
/// @param[in] bytes RTCP packet size in byte
/// @return 0-ok, <0-error
int rtp_onreceived_rtcp(void* rtp, const void* rtcp, int bytes);

/// create RTCP Report(SR/RR) packet
/// @param[in] rtp RTP object
/// @param[out] rtcp RTCP packet(include RTCP Header)
/// @param[in] bytes RTCP packet size in byte
/// @return 0-error, >0-rtcp package size(maybe need call more times)
int rtp_rtcp_report(void* rtp, void* rtcp, int bytes);

/// create RTCP BYE packet
/// @param[in] rtp RTP object
/// @param[out] rtcp RTCP packet(include RTCP Header)
/// @param[in] bytes RTCP packet size in byte
/// @return 0-error, >0-rtcp package size(maybe need call more times)
int rtp_rtcp_bye(void* rtp, void* rtcp, int bytes);

/// get RTCP interval
/// @param[in] rtp RTP object
/// 0-ok, <0-error
int rtp_rtcp_interval(void* rtp);

const char* rtp_get_cname(void* rtp, uint32_t ssrc);
const char* rtp_get_name(void* rtp, uint32_t ssrc);
int rtp_set_info(void* rtp, const char* cname, const char* name);

#ifdef __cplusplus
}
#endif
#endif /* !_rtp_h_ */
