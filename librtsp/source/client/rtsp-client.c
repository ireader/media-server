#include "rtsp-client.h"
#include "rtsp-client-internal.h"
#include "rtp-profile.h"
#include "sdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

struct rtsp_client_t* rtsp_client_create(const char* uri, const char* usr, const char* pwd, const struct rtsp_client_handler_t *handler, void* param)
{
	struct rtsp_client_t *rtsp;
	rtsp = (struct rtsp_client_t*)calloc(1, sizeof(*rtsp));
	if(NULL == rtsp)
		return NULL;

	snprintf(rtsp->uri, sizeof(rtsp->uri) - 1, "%s", uri);
	snprintf(rtsp->usr, sizeof(rtsp->usr) - 1, "%s", usr ? usr : "");
	snprintf(rtsp->pwd, sizeof(rtsp->pwd) - 1, "%s", pwd ? pwd : "");
	
	rtsp->parser = http_parser_create(HTTP_PARSER_RESPONSE, NULL, NULL);
	memcpy(&rtsp->handler, handler, sizeof(rtsp->handler));
	rtsp->rtp.onrtp = rtsp->handler.onrtp;
	rtsp->rtp.param = param;
	rtsp->state = RTSP_INIT;
	rtsp->param = param;
	rtsp->cseq = 1;
	rtsp->auth_failed = 0;

	return rtsp;
}

void rtsp_client_destroy(struct rtsp_client_t *rtsp)
{
	if (rtsp->parser)
	{
		http_parser_destroy(rtsp->parser);
		rtsp->parser = NULL;
	}

	if (rtsp->rtp.data)
	{
		assert(rtsp->rtp.capacity > 0);
		free(rtsp->rtp.data);
		rtsp->rtp.data = NULL;
		rtsp->rtp.capacity = 0;
	}

	free(rtsp);
}

static int rtsp_client_handle(struct rtsp_client_t* rtsp, http_parser_t* parser)
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

int rtsp_client_input(struct rtsp_client_t *rtsp, const void* data, size_t bytes)
{
	int r;
	size_t remain;
	const uint8_t* p, *end;

	r = 0;
	p = (const uint8_t*)data;
	end = p + bytes;

	do
	{
		if (0 == rtsp->parser_need_more_data && (*p == '$' || 0 != rtsp->rtp.state))
		{
			p = rtp_over_rtsp(&rtsp->rtp, p, end);
		}
		else
		{
			remain = (size_t)(end - p);
			r = http_parser_input(rtsp->parser, p, &remain);
			rtsp->parser_need_more_data = r;
			assert(r <= 2); // 1-need more data
			if (0 == r)
			{
				r = rtsp_client_handle(rtsp, rtsp->parser);
				http_parser_clear(rtsp->parser); // reset parser
				assert((size_t)remain < bytes);
			}
			p = end - remain;
		}
	} while (p < end && r >= 0);

	assert(r <= 1);
	return r >= 0 ? 0 : r;
}

const char* rtsp_client_get_header(struct rtsp_client_t* rtsp, const char* name)
{
	return http_get_header_by_name(rtsp->parser, name);
}

int rtsp_client_media_count(struct rtsp_client_t *rtsp)
{
	return rtsp->media_count;
}

const struct rtsp_header_transport_t* rtsp_client_get_media_transport(struct rtsp_client_t *rtsp, int media)
{
	if(media < 0 || media >= rtsp->media_count)
		return NULL;
	return rtsp->transport + media;
}

const char* rtsp_client_get_media_encoding(struct rtsp_client_t *rtsp, int media)
{
	if (media < 0 || media >= rtsp->media_count)
		return NULL;
	return rtsp->media[media].avformats[0].encoding;
}

const char* rtsp_client_get_media_fmtp(struct rtsp_client_t *rtsp, int media)
{
	if (media < 0 || media >= rtsp->media_count)
		return NULL;
	return rtsp->media[media].avformats[0].fmtp;
}

int rtsp_client_get_media_payload(struct rtsp_client_t *rtsp, int media)
{
	if (media < 0 || media >= rtsp->media_count)
		return -1;
	return rtsp->media[media].avformats[0].fmt;
}

int rtsp_client_get_media_rate(struct rtsp_client_t *rtsp, int media)
{
	int rate;
	if (media < 0 || media >= rtsp->media_count)
		return -1;
	
	rate = rtsp->media[media].avformats[0].rate;
	if (0 == rate)
	{
		const struct rtp_profile_t* profile;
		profile = rtp_profile_find(rtsp->media[media].avformats[0].fmt);
		rate = profile ? profile->frequency : 0;
	}
	return rate;
}
