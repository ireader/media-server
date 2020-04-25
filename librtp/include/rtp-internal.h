#ifndef _rtp_internal_h_
#define _rtp_internal_h_

#include "rtp.h"
#include "rtp-header.h"
#include "rtcp-header.h"
#include "rtp-member.h"
#include "rtp-member-list.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) > (b) ? (a) : (b))

struct rtp_context
{
	struct rtp_event_t handler;
	void* cbparam;

	void *members; // rtp source list
	void *senders; // rtp sender list
	struct rtp_member *self;

	// RTP/RTCP
	int avg_rtcp_size;
	int rtcp_bw;
	int rtcp_cycle; // for RTCP SDES
	int frequence;
	int init;
	int role;
};

struct rtp_member* rtp_sender_fetch(struct rtp_context *ctx, uint32_t ssrc);
struct rtp_member* rtp_member_fetch(struct rtp_context *ctx, uint32_t ssrc);

int rtcp_input_rtp(struct rtp_context *ctx, const void* data, int bytes);
int rtcp_input_rtcp(struct rtp_context *ctx, const void* data, int bytes);

int rtcp_rr_pack(struct rtp_context *ctx, uint8_t* data, int bytes);
int rtcp_sr_pack(struct rtp_context *ctx, uint8_t* data, int bytes);
int rtcp_sdes_pack(struct rtp_context *ctx, uint8_t* data, int bytes);
int rtcp_bye_pack(struct rtp_context *ctx, uint8_t* data, int bytes);
int rtcp_app_pack(struct rtp_context *ctx, uint8_t* ptr, int bytes, const char name[4], const void* app, int len);
void rtcp_rr_unpack(struct rtp_context *ctx, rtcp_header_t *header, const uint8_t* data);
void rtcp_sr_unpack(struct rtp_context *ctx, rtcp_header_t *header, const uint8_t* data);
void rtcp_sdes_unpack(struct rtp_context *ctx, rtcp_header_t *header, const uint8_t* data);
void rtcp_bye_unpack(struct rtp_context *ctx, rtcp_header_t *header, const uint8_t* data);
void rtcp_app_unpack(struct rtp_context *ctx, rtcp_header_t *header, const uint8_t* data);

uint64_t rtpclock(void);
uint64_t ntp2clock(uint64_t ntp);
uint64_t clock2ntp(uint64_t clock);

uint32_t rtp_ssrc(void);

#endif /* !_rtp_internal_h_ */
