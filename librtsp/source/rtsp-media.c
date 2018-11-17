#include "rtsp-media.h"
#include "sdp.h"
#include "sdp-a-fmtp.h"
#include "sdp-a-rtpmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
	n = strlen(base ? base : "");
	n2 = strlen(path ? path : "");
	if (n < 1 || n2 < 1 || n + n2 + 2 > bytes || !uri)
		return NULL;

	offset = snprintf(uri, bytes, "%s", base);

	if ('/' == path[0] || '\\' == path[0])
		path += 1;
	if ('/' == uri[offset - 1] || '\\' == uri[offset - 1])
		offset -= 1;
	offset += snprintf(uri + offset, bytes - offset, "/%s", path);
	return uri;
}

// rfc 2326 C.1.1 Control URL (p81)
// look for a base URL in the following order:
// 1. The RTSP Content-Base field
// 2. The RTSP Content-Location field
// 3. The RTSP request URL
int rtsp_media_set_url(struct rtsp_media_t* m, const char* base, const char* location, const char* request)
{
	char path[256] = { 0 };
	char session[256] = { 0 };

	// C.1.1 Control URL (p81)
	// If this attribute contains only an asterisk (*), then the URL is
	// treated as if it were an empty embedded URL, and thus inherits the entire base URL.
	if (*m->session_uri && '*' != *m->session_uri)
		snprintf(session, sizeof(session), "%s", m->session_uri);

	if (!isAbsoluteURL(session) && base && *base)
	{
		if (*session)
		{
			uri_join(path, sizeof(path), base, session);
			base = path;
		}
		snprintf(session, sizeof(session), "%s", base);
	}

	if (!isAbsoluteURL(session) && location && *location)
	{
		if (*session)
		{
			uri_join(path, sizeof(path), location, session);
			location = path;
		}
		snprintf(session, sizeof(session), "%s", location);
	}

	if (!isAbsoluteURL(session) && request && *request)
	{
		if (*session)
		{
			uri_join(path, sizeof(path), request, session);
			request = path;
		}
		snprintf(session, sizeof(session), "%s", request);
	}

	// update session url
	if (*m->session_uri && *session)
		snprintf(m->session_uri, sizeof(m->session_uri), "%s", session);

	// update media url
	if (!isAbsoluteURL(m->uri) && *session)
	{
		if (*m->uri)
		{
			uri_join(path, sizeof(path), session, m->uri);
			snprintf(m->uri, sizeof(m->uri), "%s", path);
		}
		else
		{
			snprintf(m->uri, sizeof(m->uri), "%s", session);
		}
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
	int payload = -1;
	struct rtsp_media_t* media;

	media = (struct rtsp_media_t*)param;

	if (name)
	{
		if (0 == strcmp("rtpmap", name))
		{
			int rate = 0;
			char encoding[sizeof(media->avformats[i].encoding)];
			if (strlen(value) < sizeof(encoding)) // make sure encoding have enough memory space
			{
				sdp_a_rtpmap(value, &payload, encoding, &rate, NULL);
				for (i = 0; i < media->avformat_count; i++)
				{
					if (media->avformats[i].fmt == payload)
					{
						media->avformats[i].rate = rate;
						snprintf(media->avformats[i].encoding, sizeof(media->avformats[i].encoding), "%s", encoding);
						break;
					}
				}
			}
		}
		else if (0 == strcmp("fmtp", name))
		{
			payload = atoi(value);
			for (i = 0; i < media->avformat_count; i++)
			{
				if (media->avformats[i].fmt != payload)
					continue;

				snprintf(media->avformats[i].fmtp, sizeof(media->avformats[i].fmtp), "%s", value);
				//if(0 == strcmp("H264", media->avformats[i].encoding))
				//{
				//	struct sdp_a_fmtp_h264_t h264;
				//	memset(&h264, 0, sizeof(h264));
				//	sdp_a_fmtp_h264(value, &payload, &h264);
				//	if(h264.flags & SDP_A_FMTP_H264_SPROP_PARAMETER_SETS)
				//		snprintf(media->avformats[i].ps, sizeof(media->avformats[i].ps), "%s", h264.sprop_parameter_sets);
				//}
				//else if (0 == strcmp("H265", media->avformats[i].encoding))
				//{
				//	struct sdp_a_fmtp_h265_t h265;
				//	memset(&h265, 0, sizeof(h265));
				//	sdp_a_fmtp_h265(value, &payload, &h265);
				//	//if (h265.flags & SDP_A_FMTP_H265_SPROP_VPS)
				//	//	snprintf(media->avformats[i].ps, sizeof(media->avformats[i].ps), "%s", h265.sprop_vps);
				//	//if (h265.flags & SDP_A_FMTP_H265_SPROP_SPS)
				//	//	snprintf(media->avformats[i].ps, sizeof(media->avformats[i].ps), "%s", h265.sprop_sps); 
				//	//if (h265.flags & SDP_A_FMTP_H265_SPROP_PPS)
				//	//	snprintf(media->avformats[i].ps, sizeof(media->avformats[i].ps), "%s", h265.sprop_pps);
				//}
				//else if(0 == strcmp("mpeg4-generic", media->avformats[i].encoding))
				//{
				//	struct sdp_a_fmtp_mpeg4_t mpeg4;
				//	memset(&mpeg4, 0, sizeof(mpeg4));
				//	sdp_a_fmtp_mpeg4(value, &payload, &mpeg4);
				//}
				break;
			}
		}
		else if (0 == strcmp("etag", name))
		{
			// C.1.8 Entity Tag
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
int rtsp_media_sdp(const char* s, struct rtsp_media_t* medias, int count)
{
	int i, j, n;
	int formats[3];
	const char* control;
	const char* start, *stop;
	const char* network, *addrtype, *address;
	struct rtsp_media_t* m;
	struct rtsp_header_range_t range;
	sdp_t* sdp;

	sdp = sdp_parse(s);
	if (!sdp)
		return -1;

	// rfc 2326 C.1.1 Control URL (p80)
	// If found at the session level, the attribute indicates the URL for aggregate control
	control = sdp_attribute_find(sdp, "control");

	// C.1.5 Range of presentation
	// The "a=range" attribute defines the total time range of the stored session.
	memset(&range, 0, sizeof(range));
	s = sdp_attribute_find(sdp, "range");
	if(s) rtsp_header_range(s, &range);

	// C.1.6 Time of availability
	start = stop = NULL;
	for (i = 0; i < sdp_timing_count(sdp); i++)
	{
	//	sdp_timing_get(sdp, i, &start, &stop);
	}

	// C.1.7 Connection Information
	network = addrtype = address = NULL;
	sdp_connection_get(sdp, &network, &addrtype, &address);

	for (i = 0; i < sdp_media_count(sdp) && i < count; i++)
	{
		m = medias + i;
		memset(m, 0, sizeof(struct rtsp_media_t));
		memcpy(&m->range, &range, sizeof(m->range));
		if (control)
			snprintf(m->session_uri, sizeof(m->session_uri), "%s", control);
		if (start && stop)
		{
			m->start = strtoull(start, NULL, 10);
			m->stop = strtoull(stop, NULL, 10);
		}
		snprintf(m->network, sizeof(m->network), "%s", network);
		snprintf(m->addrtype, sizeof(m->addrtype), "%s", addrtype);
		snprintf(m->address, sizeof(m->address), "%s", address);
		//media->cseq = rand();

		// media control url
		s = sdp_media_attribute_find(sdp, i, "control");
		if(s)
			snprintf(m->uri, sizeof(m->uri), "%s", s);

		// media format
		j = sizeof(m->avformats) / sizeof(m->avformats[0]);
		assert(sizeof(formats) / sizeof(formats[0]) >= j);
		n = sdp_media_formats(sdp, i, formats, j);
		m->avformat_count = n > j ? j : n;
		for (j = 0; j < m->avformat_count; j++)
			m->avformats[j].fmt = formats[j];

		// update media encoding
		sdp_media_attribute_list(sdp, i, NULL, rtsp_media_onattr, m);
	}

	count = sdp_media_count(sdp);
	sdp_destroy(sdp);
	return count; // should check return value
}
