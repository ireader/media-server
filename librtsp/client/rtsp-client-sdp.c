#include "rtsp-client-internal.h"
#include "sdp.h"
#include "sdp-a-fmtp.h"
#include "sdp-a-rtpmap.h"

static inline int rtsp_media_aggregate_control_enable(void *sdp)
{
	const char* control;

	// rfc 2326 C.1.1 Control URL (p80)
	// If found at the session level, the attribute indicates the URL for aggregate control
	control = sdp_attribute_find(sdp, "control");
	return (control && *control) ? 1 : 0;
}

static int isAbsoluteURL(char const* url) 
{
	// Assumption: "url" is absolute if it contains a ':', before any
	// occurrence of '/'
	while (*url != '\0' && *url != '/') {
		if (*url == ':') return 1;
		++url;
	}

	return 0;
}

static const char* uri_join(char* uri, size_t bytes, const char* base, const char* path)
{
	size_t n, n2, offset;

	assert(uri && base && path);
	n = strlen(base);
	n2 = strlen(path);
	if(n + n2 + 1 > bytes || !uri)
		return NULL;

	offset = strlcpy(uri, base, bytes);

	if( ('/' == path[0] || '\\' == path[0])
		&& n > 0 && ('/' == uri[n-1] || '\\' == uri[n-1]) )
	{
		offset += strlcat(uri + offset, path+1, bytes - offset);
	}
	else
	{
		offset += strlcat(uri + offset, path, bytes - offset);
	}

	return uri;
}

// rfc 2326 C.1.1 Control URL (p81)
// look for a base URL in the following order:
// 1. The RTSP Content-Base field
// 2. The RTSP Content-Location field
// 3. The RTSP request URL
static int rtsp_get_session_uri(void *sdp, char* uri, size_t bytes, const char* requri, const char* baseuri, const char* location)
{
	char path[256] = {0};
	const char* control;

	// C.1.1 Control URL (p81)
	// If this attribute contains only an asterisk (*), then the URL is
	// treated as if it were an empty embedded URL, and thus inherits the entire base URL.
	control = sdp_attribute_find(sdp, "control");
	if(!control || 0==*control || '*' == *control)
		control = "";
	strlcpy(uri, control, bytes);

	if(!isAbsoluteURL(uri) && baseuri && *baseuri)
	{
		if(*uri)
		{
			uri_join(path, sizeof(path), baseuri, uri);
			baseuri = path;
		}
		strlcpy(uri, baseuri, bytes);	
	}

	if(!isAbsoluteURL(uri) && location && *location)
	{
		if(*uri)
		{
			uri_join(path, sizeof(path), location, uri);
			location = path;
		}
		strlcpy(uri, location, bytes);
	}

	if(!isAbsoluteURL(uri) && requri && *requri)
	{
		if(*uri)
		{
			uri_join(path, sizeof(path), requri, uri);
			requri = path;
		}
		strlcpy(uri, requri, bytes);
	}

	return 0;
}

static int rtsp_get_media_uri(void *sdp, int media, char* uri, size_t bytes, const char* sessionuri)
{
	char path[256] = {0};
	const char* control;

	// C.1.1 Control URL (p81)
	// If this attribute contains only an asterisk (*), then the URL is
	// treated as if it were an empty embedded URL, and thus inherits the entire base URL.
	control = sdp_media_attribute_find(sdp, media, "control");
	if(!control || 0==*control || '*' == *control)
		control = "";
	strlcpy(uri, control, bytes-1);

	if(!isAbsoluteURL(uri) && sessionuri && *sessionuri)
	{
		if(*uri)
		{
			uri_join(path, sizeof(path), sessionuri, uri);
			sessionuri = path;
		}
		strlcpy(uri, sessionuri, bytes);
	}

	return 0;
}

// RFC 6184 RTP Payload Format for H.264 Video
// 8.2.1. Mapping of Payload Type Parameters to SDP
// m=video 49170 RTP/AVP 98
// a=rtpmap:98 H264/90000
// a=fmtp:98 profile-level-id=42A01E;
//			 packetization-mode=1;
//			 sprop-parameter-sets=<parameter sets data>
static void rtsp_media_onattr(void* param, const char* name, const char* value)
{
	int i;
	struct rtsp_media_t* media;

	media = (struct rtsp_media_t*)param;

	if(name)
	{
		if(0 == strcmp("rtpmap", name))
		{
			int payload = -1;
			char encoding[32];
			if(strlen(value) < sizeof(encoding)) // make sure encoding have enough memory space
			{
				sdp_a_rtpmap(value, &payload, encoding, NULL, NULL);
				for(i = 0; i < media->avformat_count; i++)
				{
					if(media->avformats[i].pt == payload)
					{
						strlcpy(media->avformats[i].encoding, encoding, sizeof(media->avformats[i].encoding));
						break;
					}
				}
			}
		}
		else if(0 == strcmp("fmtp", name))
		{
			int payload = -1;
			payload = atoi(value);
			for(i = 0; i < media->avformat_count; i++)
			{
				if(media->avformats[i].pt == payload)
				{
					if(0 == strcmp("H264", media->avformats[i].encoding))
					{
						struct sdp_a_fmtp_h264_t h264;
						memset(&h264, 0, sizeof(h264));
						sdp_a_fmtp_h264(value, &payload, &h264);
						if(h264.flags & SDP_A_FMTP_H264_SPROP_PARAMETER_SETS)
						{
							strlcpy(media->avformats[i].spspps, h264.sprop_parameter_sets, sizeof(media->avformats[i].spspps));
						}
					}
					else if(0 == strcmp("mpeg4-generic", media->avformats[i].encoding))
					{
						struct sdp_a_fmtp_mpeg4_t mpeg4;
						memset(&mpeg4, 0, sizeof(mpeg4));
						sdp_a_fmtp_mpeg4(value, &payload, &mpeg4);
					}
					break;
				}
			}
		}
	}
}

/*
v=0
o=mhandley 2890844526 2890842807 IN IP4 126.16.64.4
s=SDP Seminar
i=A Seminar on the session description protocol
u=http://www.cs.ucl.ac.uk/staff/M.Handley/sdp.03.ps
e=mjh@isi.edu (Mark Handley)
c=IN IP4 224.2.17.12/127
t=2873397496 2873404696
a=recvonly
m=audio 3456 RTP/AVP 0
m=video 2232 RTP/AVP 31
m=whiteboard 32416 UDP WB
a=orient:portrait
*/
int rtsp_client_sdp(struct rtsp_client_context_t* ctx, void* sdp)
{
	int i, count;
	int formats[N_MEDIA_FORMAT];
	struct rtsp_media_t* media;

	assert(sdp);
	count = sdp_media_count(sdp);
	if(count > N_MEDIA)
	{
		ctx->media_ptr = (struct rtsp_media_t*)malloc(sizeof(struct rtsp_media_t)*(count-N_MEDIA));
		if(!ctx->media_ptr)
			return -1;
		memset(ctx->media_ptr, 0, sizeof(struct rtsp_media_t)*(count-N_MEDIA));
	}

	ctx->media_count = count;

	// rfc 2326 C.1.1 Control URL (p80)
	// If found at the session level, the attribute indicates the URL for aggregate control
	ctx->aggregate = rtsp_media_aggregate_control_enable(sdp);
	rtsp_get_session_uri(sdp, ctx->aggregate_uri, sizeof(ctx->aggregate_uri), ctx->uri, ctx->baseuri, ctx->location);

	for(i = 0; i < count; i++)
	{
		int j, n;
		media = rtsp_get_media(ctx, i);
		media->cseq = rand();

		// RTSP2326 C.1.1 Control URL
		rtsp_get_media_uri(sdp, i, media->uri, sizeof(media->uri), ctx->aggregate_uri);

		n = sdp_media_formats(sdp, i, formats, N_MEDIA_FORMAT);
		media->avformat_count = n > N_MEDIA_FORMAT ? N_MEDIA_FORMAT : n;
		for(j = 0; j < media->avformat_count; j++)
		{
			media->avformats[j].pt = formats[j];
		}

		// update media encoding
		sdp_media_attribute_list(sdp, i, NULL, rtsp_media_onattr, media);		
	}

	return 0;
}
