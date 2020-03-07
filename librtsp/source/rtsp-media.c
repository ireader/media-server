#include "rtsp-media.h"
#include "sdp.h"
#include "sdp-a-fmtp.h"
#include "sdp-a-rtpmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

static inline int scopy(struct rtsp_media_t* medias, char** dst, const char* src)
{
	int n;
	n = snprintf(medias->ptr + medias->offset, sizeof(medias->ptr) - medias->offset, "%s", src);
	if (n < 0 || n >= (int)sizeof(medias->ptr) - medias->offset)
		return -1;
	*dst = medias->ptr + medias->offset;
	medias->offset += n + 1; // with '\0'
	return 0;
}

//static inline int vscopy(struct rtsp_media_t* medias, char** dst, const char* fmt, ...)
//{
//	int n;
//	va_list args;
//	va_start(args, fmt);
//	n = vsnprintf(medias->ptr + medias->offset, sizeof(medias->ptr) - medias->offset, fmt, args);
//	va_end(args);
//
//	if (n < 0 || n >= (int)sizeof(medias->ptr) - medias->offset)
//		return -1;
//	*dst = medias->ptr + medias->offset;
//	medias->offset += n + 1; // with '\0'
//	return 0;
//}
//
//static inline int rtsp_media_aggregate_control_enable(void *sdp)
//{
//	const char* control;
//
//	// rfc 2326 C.1.1 Control URL (p80)
//	// If found at the session level, the attribute indicates the URL for aggregate control
//	control = sdp_attribute_find(sdp, "control");
//	return (control && *control) ? 1 : 0;
//}

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
	int i, n;
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
			n = (int)strlen(value);
			payload = atoi(value);
			for (i = 0; i < media->avformat_count && media->offset + n + 1 < sizeof(media->ptr); i++)
			{
				if (media->avformats[i].fmt != payload)
					continue;

				media->avformats[i].fmtp = media->ptr + media->offset;
				strcpy(media->avformats[i].fmtp, value);
				media->offset += n + 1;
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
		else if (0 == strcmp("rtcp", name))
		{
			// rfc3605 Real Time Control Protocol (RTCP) attribute in Session Description Protocol (SDP)
			// "a=rtcp:" port [nettype space addrtype space connection-address] CRLF
			// a=rtcp:53020 IN IP6 2001:2345:6789:ABCD:EF01:2345:6789:ABCD
			assert(media->nport <= 2 && sizeof(media->port)/sizeof(media->port[0]) >= 2);
			if (1 == media->nport)
				media->nport++;
			media->port[1] = atoi(value);

			// TODO: rtcp address
		}
		else if (0 == strcmp("rtcp-mux", name))
		{
			if (1 == media->nport)
				media->nport++;
			media->port[1] = media->port[0];
		}
		else if (0 == strcmp("rtcp-xr", name))
		{
			// rfc3611 RTP Control Protocol Extended Reports (RTCP XR)
			// "a=rtcp-xr:" [xr-format *(SP xr-format)] CRLF
		}
		else if (0 == strcmp("ice-pwd", name))
		{
			scopy(media, &media->ice.pwd, value);
		}
		else if (0 == strcmp("ice-ufrag", name))
		{
			scopy(media, &media->ice.ufrag, value);
		}
		else if (0 == strcmp("ice-lite", name))
		{
			media->ice.lite = 1;
		}
		else if (0 == strcmp("ice-mismatch", name))
		{
			media->ice.mismatch = 1;
		}
		else if (0 == strcmp("ice-pacing", name))
		{
			media->ice.pacing = atoi(value);
		}
		else if (0 == strcmp("candidate", name))
		{
			if (media->ice.candidate_count + 1 < sizeof(media->ice.candidates) / sizeof(media->ice.candidates[0]) && media->offset + sizeof(*media->ice.candidates[0]) <= sizeof(media->ptr))
			{
				media->ice.candidates[media->ice.candidate_count] = (struct sdp_candidate_t*)(media->ptr + media->offset);
				if (7 == sscanf(value, "%32s %hu %7s %u %63s %hu typ %7s%n",
					media->ice.candidates[media->ice.candidate_count]->foundation,
					&media->ice.candidates[media->ice.candidate_count]->component,
					media->ice.candidates[media->ice.candidate_count]->transport,
					&media->ice.candidates[media->ice.candidate_count]->priority,
					media->ice.candidates[media->ice.candidate_count]->address,
					&media->ice.candidates[media->ice.candidate_count]->port,
					media->ice.candidates[media->ice.candidate_count]->candtype, &n))
				{
					sscanf(value + n, " raddr %63s rport %hu", media->ice.candidates[media->ice.candidate_count]->reladdr, &media->ice.candidates[media->ice.candidate_count]->relport);
					media->offset += sizeof(*media->ice.candidates[0]);
					++media->ice.candidate_count;
				}
			}
		}
		else if (0 == strcmp("remote-candidates", name))
		{
			while (media->ice.remote_count + 1 < sizeof(media->ice.remotes) / sizeof(media->ice.remotes[0]) && media->offset + sizeof(*media->ice.remotes[0]) <= sizeof(media->ptr))
			{
				media->ice.remotes[media->ice.remote_count] = (struct sdp_candidate_t*)(media->ptr + media->offset);
				if (!value || 3 != sscanf(value, "%hu %63s %hu%n", &media->ice.remotes[media->ice.remote_count]->component, &media->ice.remotes[media->ice.remote_count]->address, &media->ice.remotes[media->ice.remote_count]->port, &n))
					break;

				value += n;
				++media->ice.remote_count;
				media->offset += sizeof(*media->ice.remotes[0]);
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
int rtsp_media_sdp(const char* s, struct rtsp_media_t* medias, int count)
{
	int i, j, n;
	int formats[16];
	const char* control;
	const char* start, *stop;
	const char* iceufrag, *icepwd;
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

	// session ice-ufrag/ice-pwd
	iceufrag = sdp_attribute_find(sdp, "ice-ufrag");
	icepwd = sdp_attribute_find(sdp, "ice-pwd");

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
		
		m->nport = sdp_media_port(sdp, i, m->port, sizeof(m->port)/sizeof(m->port[0]));
		snprintf(m->media, sizeof(m->media), "%s", sdp_media_type(sdp, i));
		snprintf(m->proto, sizeof(m->proto), "%s", sdp_media_proto(sdp, i));
		if (1 == m->nport && 0 == strncmp("RTP/", m->proto, 4))
			m->port[m->nport++] = m->port[0] + 1;

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

		// use default ice-ufrag/pwd
		if(NULL == m->ice.ufrag && iceufrag)
			scopy(medias, &m->ice.ufrag, iceufrag);
		if(NULL == m->ice.pwd && icepwd)
			scopy(medias, &m->ice.pwd, icepwd);
	}

	count = sdp_media_count(sdp);
	sdp_destroy(sdp);
	return count; // should check return value
}
