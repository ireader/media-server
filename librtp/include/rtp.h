#ifndef _rtp_h_
#define _rtp_h_

#include <stdint.h>
#include "rtcp-header.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rtcp_msg_t
{
	int type;
	uint32_t ssrc; // rtcp message sender

	union rtcp_msg_u
	{
		// type = RTCP_SR
		rtcp_rb_t sr;

		// type = RTCP_SR
		rtcp_rb_t rr;

		// type = RTCP_SDES
		rtcp_sdes_item_t sdes;

		// type = RTCP_BYE
		rtcp_bye_t bye;

		// type = RTCP_APP
		rtcp_app_t app;

		// type = RTCP_RTPFB | (RTCP_RTPFB_NACK << 8)
		rtcp_rtpfb_t rtpfb;

		// type = RTCP_PSFB | (RTCP_PSFB_PLI << 8)
		rtcp_psfb_t psfb;

		// type = RTCP_XR | (RTCP_XR_DLRR << 8)
		rtcp_xr_t xr;
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

/// create RTCP APP packet
/// @param[in] rtp RTP object
/// @param[out] rtcp RTCP packet(include RTCP Header)
/// @param[in] bytes RTCP packet size in byte
/// @return 0-error, >0-rtcp package size(maybe need call more times)
int rtp_rtcp_app(void* rtp, void* rtcp, int bytes, const char name[4], const void* app, int len);

/// create RTCP RTPFB packet
/// @param[in] rtp RTP object
/// @param[out] data RTCP packet(include RTCP Header)
/// @param[in] bytes RTCP packet size in byte
/// @param[in] id FMT Values for RTPFB Payload Types
/// @param[in] rtpfb RTPFB info
/// @return 0-error, >0-rtcp package size(maybe need call more times)
int rtp_rtcp_rtpfb(void* rtp, void* data, int bytes, enum rtcp_rtpfb_type_t id, const rtcp_rtpfb_t *rtpfb);

/// create RTCP PSFB packet
/// @param[in] rtp RTP object
/// @param[out] rtcp RTCP packet(include RTCP Header)
/// @param[in] bytes RTCP packet size in byte
/// @param[in] id FMT Values for PSFB Payload Types
/// @param[in] psfb PSFB info
/// @return 0-error, >0-rtcp package size(maybe need call more times)
int rtp_rtcp_psfb(void* rtp, void* data, int bytes, enum rtcp_psfb_type_t id, const rtcp_psfb_t* psfb);

/// create RTCP XR packet
/// @param[in] rtp RTP object
/// @param[out] data RTCP packet(include RTCP Header)
/// @param[in] bytes RTCP packet size in byte
/// @param[in] id FMT Values for XR Payload Types
/// @param[in] xr XR info
/// @return 0-error, >0-rtcp package size(maybe need call more times)
int rtp_rtcp_xr(void* rtp, void* data, int bytes, enum rtcp_xr_type_t id, const rtcp_xr_t* xr);

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
