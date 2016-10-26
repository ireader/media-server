#ifndef _rtp_internal_h_
#define _rtp_internal_h_

#include <stdlib.h>
#include "time64.h"
#include "ctypedef.h"
#include "cstringext.h"
#include "rtp.h"
#include "rtp-header.h"
#include "rtcp-header.h"
#include "rtp-member.h"
#include "rtp-member-list.h"

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
	size_t avg_rtcp_size;
	size_t rtcp_bw;
	size_t rtcp_cycle; // for RTCP SDES
	size_t frequence;
	int init;
	int role;
};

struct rtp_member* rtp_sender_fetch(struct rtp_context *ctx, uint32_t ssrc);
struct rtp_member* rtp_member_fetch(struct rtp_context *ctx, uint32_t ssrc);

int rtcp_input_rtp(struct rtp_context *ctx, const void* data, size_t bytes);
int rtcp_input_rtcp(struct rtp_context *ctx, const void* data, size_t bytes);

size_t rtcp_rr_pack(struct rtp_context *ctx, unsigned char* data, size_t bytes);
size_t rtcp_sr_pack(struct rtp_context *ctx, unsigned char* data, size_t bytes);
size_t rtcp_sdes_pack(struct rtp_context *ctx, unsigned char* data, size_t bytes);
size_t rtcp_bye_pack(struct rtp_context *ctx, unsigned char* data, size_t bytes);
size_t rtcp_app_pack(struct rtp_context *ctx, unsigned char* ptr, size_t bytes, const char name[4], const void* app, size_t len);
void rtcp_rr_unpack(struct rtp_context *ctx, rtcp_header_t *header, const unsigned char* data);
void rtcp_sr_unpack(struct rtp_context *ctx, rtcp_header_t *header, const unsigned char* data);
void rtcp_sdes_unpack(struct rtp_context *ctx, rtcp_header_t *header, const unsigned char* data);
void rtcp_bye_unpack(struct rtp_context *ctx, rtcp_header_t *header, const unsigned char* data);
void rtcp_app_unpack(struct rtp_context *ctx, rtcp_header_t *header, const unsigned char* data);

#endif /* !_rtp_internal_h_ */
