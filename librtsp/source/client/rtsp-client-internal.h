#ifndef _rtsp_client_internal_h_
#define _rtsp_client_internal_h_

#include "rtsp-client.h"
#include "rtp-over-rtsp.h"
#include "http-header-auth.h"
#include "rtsp-header-session.h"
#include "rtsp-header-transport.h"
#include "rtsp-parser.h"
#include "sdp.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#if defined(OS_WINDOWS)
#define strcasecmp	_stricmp
#endif

#define USER_AGENT "RTSP client v0.1"
#define N_MEDIA 2
#define N_MEDIA_FORMAT 3

struct rtsp_media_t
{
	char uri[256]; // rtsp setup

	//unsigned int cseq; // rtsp sequence, unused if aggregate control available
	struct rtsp_header_session_t session;
	struct rtsp_header_transport_t transport;

	int avformat_count;
	struct avformat_t
	{
		int fmt; // RTP payload type
		int rate; // RTP payload frequency
		int channel; // RTP payload channel
		char encoding[64]; // RTP payload encoding
		char spspps[128]; // H.264 only
	} avformats[N_MEDIA_FORMAT];
};

enum rtsp_state_t
{
	RTSP_INIT,
	RTSP_ANNOUNCE,
	RTSP_DESCRIBE,
	RTSP_SETUP,
	RTSP_PLAY,
	RTSP_PAUSE,
	RTSP_TEARDWON,
	RTSP_OPTIONS,
	RTSP_GET_PARAMETER,
	RTSP_SET_PARAMETER,
};

struct rtsp_client_t
{
	struct rtsp_client_handler_t handler;
	void* param;

	rtsp_parser_t* parser;
	enum rtsp_state_t state;
	int parser_need_more_data;
	int progress;
	unsigned int cseq; // rtsp sequence

	struct rtp_over_rtsp_t rtp;

	sdp_t* sdp;
	int media_count;
	struct rtsp_media_t media[N_MEDIA];
	struct rtsp_media_t *media_ptr;

	// media
	char range[64]; // rtsp header Range
	char speed[16]; // rtsp header speed
	char req[2048];

	char uri[256];
	char baseuri[256]; // Content-Base
	char location[256]; // Content-Location

	int aggregate; // 1-aggregate control available
	char aggregate_uri[256]; // aggregate control uri, valid if 1==aggregate

	int auth_failed;
	char usr[128];
	char pwd[256];
	char cnonce[32];
	char nc[32];
	char authenrization[1024];
	struct http_header_www_authenticate_t auth;
};

static inline struct rtsp_media_t* rtsp_get_media(struct rtsp_client_t *ctx, int i)
{
	if(i < 0 || i >= ctx->media_count)
		return NULL;

	return i < N_MEDIA ? (ctx->media + i) : (ctx->media_ptr + i - N_MEDIA);
}

//int rtsp_client_describe(struct rtsp_client_t* rtsp);
//int rtsp_client_announce(struct rtsp_client_t* rtsp, const char* sdp);
//int rtsp_client_setup(struct rtsp_client_t* rtsp, const char* sdp);
//int rtsp_client_teardown(struct rtsp_client_t* rtsp);
int rtsp_client_sdp(struct rtsp_client_t* rtsp, const char* sdp);
int rtsp_client_options(struct rtsp_client_t *rtsp, const char* commands);
int rtsp_client_get_parameter(struct rtsp_client_t *rtsp, int media, const char* parameter);
int rtsp_client_set_parameter(struct rtsp_client_t *rtsp, int media, const char* parameter);

int rtsp_client_announce_onreply(struct rtsp_client_t* rtsp, void* parser);
int rtsp_client_describe_onreply(struct rtsp_client_t* rtsp, void* parser);
int rtsp_client_setup_onreply(struct rtsp_client_t* rtsp, void* parser);
int rtsp_client_play_onreply(struct rtsp_client_t* rtsp, void* parser);
int rtsp_client_pause_onreply(struct rtsp_client_t* rtsp, void* parser);
int rtsp_client_teardown_onreply(struct rtsp_client_t* rtsp, void* parser);
int rtsp_client_options_onreply(struct rtsp_client_t* rtsp, void* parser);
int rtsp_client_get_parameter_onreply(struct rtsp_client_t* rtsp, void* parser);
int rtsp_client_set_parameter_onreply(struct rtsp_client_t* rtsp, void* parser);

int rtsp_client_www_authenticate(struct rtsp_client_t* rtsp, const char* filed);
int rtsp_client_authenrization(struct rtsp_client_t* rtsp, const char* method, const char* uri, const char* content, int length, char* authenrization, int bytes);

#endif /* !_rtsp_client_internal_h_ */
