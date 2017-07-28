#include "rtsp-client.h"
#include "rtsp-client-internal.h"
#include "rtsp-parser.h"
#include "rtp-profile.h"
#include "sdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

struct rtsp_client_t* rtsp_client_create(const char* usr, const char* pwd, const struct rtsp_client_handler_t *handler, void* param)
{
	struct rtsp_client_t *rtsp;
	rtsp = (struct rtsp_client_t*)calloc(1, sizeof(*rtsp));
	if(NULL == rtsp)
		return NULL;

	snprintf(rtsp->usr, sizeof(rtsp->usr), "%s", usr ? usr : "");
	snprintf(rtsp->pwd, sizeof(rtsp->pwd), "%s", pwd ? pwd : "");
	snprintf(rtsp->nc, sizeof(rtsp->nc), "%08x", 1);
	snprintf(rtsp->cnonce, sizeof(rtsp->cnonce), "%p", rtsp);

	memcpy(&rtsp->handler, handler, sizeof(rtsp->handler));
	rtsp->state = RTSP_INIT;
	rtsp->param = param;
	rtsp->cseq = 1;

	return rtsp;
}

void rtsp_client_destroy(struct rtsp_client_t *rtsp)
{
	if(rtsp->media_ptr)
		free(rtsp->media_ptr);

	free(rtsp);
}

int rtsp_client_open(struct rtsp_client_t *rtsp, const char* uri, const char* sdp)
{
	int r;
	rtsp->auth_failed = 0;
	strlcpy(rtsp->uri, uri, sizeof(rtsp->uri));
	if(NULL == sdp || 0 == *sdp)
		r = rtsp_client_describe(rtsp);
	else
		r = rtsp_client_setup(rtsp, sdp);
	return r;
}

int rtsp_client_close(struct rtsp_client_t *rtsp)
{
	assert(RTSP_TEARDWON != rtsp->state);
	return rtsp_client_teardown(rtsp);
}

int rtsp_client_input(struct rtsp_client_t *rtsp, void* parser)
{
	switch (rtsp->state)
	{
	case RTSP_ANNOUNCE:	return rtsp_client_announce_onreply(rtsp, parser);
	case RTSP_DESCRIBE: return rtsp_client_describe_onreply(rtsp, parser);
	case RTSP_SETUP:	return rtsp_client_setup_onreply(rtsp, parser);
	case RTSP_PLAY:		return rtsp_client_play_onreply(rtsp, parser);
	case RTSP_PAUSE:	return rtsp_client_pause_onreply(rtsp, parser);
	case RTSP_TEARDWON: return rtsp_client_teardown_onreply(rtsp, parser);
	case RTSP_OPTIONS:	return rtsp_client_options_onreply(rtsp, parser);
	case RTSP_GET_PARAMETER: return rtsp_client_get_parameter_onreply(rtsp, parser);
	case RTSP_SET_PARAMETER: return rtsp_client_set_parameter_onreply(rtsp, parser);
	default: assert(0); return -1;
	}
}

int rtsp_client_media_count(struct rtsp_client_t *rtsp)
{
	return rtsp->media_count;
}

const struct rtsp_header_transport_t* rtsp_client_get_media_transport(struct rtsp_client_t *rtsp, int media)
{
	if(media < 0 || media >= rtsp->media_count)
		return NULL;
	return &rtsp_get_media(rtsp, media)->transport;
}

const char* rtsp_client_get_media_encoding(struct rtsp_client_t *rtsp, int media)
{
	if (media < 0 || media >= rtsp->media_count)
		return NULL;
	return rtsp_get_media(rtsp, media)->avformats[0].encoding;
}

int rtsp_client_get_media_payload(struct rtsp_client_t *rtsp, int media)
{
	if (media < 0 || media >= rtsp->media_count)
		return -1;
	return rtsp_get_media(rtsp, media)->avformats[0].fmt;
}

int rtsp_client_get_media_rate(struct rtsp_client_t *rtsp, int media)
{
	int rate;
	if (media < 0 || media >= rtsp->media_count)
		return -1;
	
	rate = rtsp_get_media(rtsp, media)->avformats[0].rate;
	if (0 == rate)
	{
		const struct rtp_profile_t* profile;
		profile = rtp_profile_find(rtsp_get_media(rtsp, media)->avformats[0].fmt);
		rate = profile ? profile->frequency : 0;
	}
	return rate;
}
