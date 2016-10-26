#ifndef _rtsp_client_internal_h_
#define _rtsp_client_internal_h_

#include "rtsp-client.h"
#include "rtsp-header-transport.h"
#include "rtsp-parser.h"
#include "cstringext.h"

#define USER_AGENT "Netposa RTSP Lib"
#define N_MEDIA 2
#define N_MEDIA_FORMAT 3

struct rtsp_media_t
{
	char uri[256]; // rtsp setup

	unsigned int cseq; // rtsp sequence, unused if aggregate control available
	char session[128]; // rtsp session, empty if Aggregate Control Available

	struct rtsp_header_transport_t transport;

	int avformat_count;
	struct avformat_t
	{
		int pt;
		char encoding[32];
		char spspps[512];
	} avformats[N_MEDIA_FORMAT];
};

enum {
	RTSP_CREATE,
	RTSP_DESCRIBE,
	RTSP_SETUP,
	RTSP_PLAY,
	RTSP_PAUSE,
	RTSP_TEARDWON,
};

struct rtsp_client_context_t
{
	rtsp_client_t client;
	void* transport;
	void* param;

	int status;
	int progress;
	unsigned int cseq; // rtsp sequence

	int media_count;
	struct rtsp_media_t media[N_MEDIA];
	struct rtsp_media_t *media_ptr;

	int aggregate; // 1-aggregate control available
	char aggregate_uri[256]; // aggregate control uri, valid if 1==aggregate

	char range[64]; // rtsp header Range
	char speed[16]; // rtsp header speed
	char req[1024];
	char *ptr;
	int bytes;

	char uri[256];
	char baseuri[256]; // Content-Base
	char location[256]; // Content-Location
};

static inline struct rtsp_media_t* rtsp_get_media(struct rtsp_client_context_t *ctx, int i)
{
	if(i < 0 || i >= ctx->media_count)
		return NULL;

	return i < N_MEDIA ? (ctx->media + i) : (ctx->media_ptr + i - N_MEDIA);
}

int rtsp_client_announce(struct rtsp_client_context_t* ctx, const char* sdp);
int rtsp_client_describe(struct rtsp_client_context_t* ctx, const char* uri);
int rtsp_client_sdp(struct rtsp_client_context_t* ctx, void* sdp);

int rtsp_client_media_setup(struct rtsp_client_context_t* ctx);
int rtsp_client_media_play(struct rtsp_client_context_t *ctx);
int rtsp_client_media_pause(struct rtsp_client_context_t *ctx);
int rtsp_client_media_teardown(struct rtsp_client_context_t* ctx);

#endif /* !_rtsp_client_internal_h_ */
