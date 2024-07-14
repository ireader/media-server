#include "rtsp-media.h"
#include "sdp.h"
#include "sdp-options.h"
#include "sdp-a-fmtp.h"
#include "sdp-a-rtpmap.h"
#include "sys/path.h"
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

// rfc 2326 C.1.1 Control URL (p81)
// look for a base URL in the following order:
// 1. The RTSP Content-Base field
// 2. The RTSP Content-Location field
// 3. The RTSP request URL
int rtsp_media_set_url(struct rtsp_media_t* m, const char* base, const char* location, const char* request)
{
	int r;
	char buffer[256] = { 0 };

	// C.1.1 Control URL (p81)
	// If this attribute contains only an asterisk (*), then the URL is
	// treated as if it were an empty embedded URL, and thus inherits the entire base URL.
	if (m->session_uri[0] && '*' != m->session_uri[0])
	{
		snprintf(buffer, sizeof(buffer)-1, "%s", m->session_uri);
		r = path_resolve2(m->session_uri, sizeof(m->session_uri)-1, buffer, base, location, request);
	}
	else if('*' == m->session_uri[0])
	{
		r = snprintf(m->session_uri, sizeof(m->session_uri) - 1, "%s", request);
	}
	else
	{
		// keep session uri empty
		r = 0;
	}

	if ('*' != m->uri[0])
	{
		snprintf(buffer, sizeof(buffer) - 1, "%s", m->uri);
		r = path_resolve2(m->uri, sizeof(m->uri) - 1, buffer, base, location, request);
	}
	else
	{
		r = snprintf(m->uri, sizeof(m->uri) - 1, "%s", request);
	}

	return r >= 0 ? 0 : r;
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
			char channel[64];
			char encoding[16 + sizeof(media->avformats[i].encoding)];
			if (strlen(value) < sizeof(encoding)) // make sure encoding have enough memory space
			{
				channel[0] = '\0';
				sdp_a_rtpmap(value, &payload, encoding, &rate, channel);
				for (i = 0; i < media->avformat_count; i++)
				{
					if (media->avformats[i].fmt == payload)
					{
						media->avformats[i].rate = rate;
						media->avformats[i].channel = *channel ? atoi(channel) : 1; // default 1-channel
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
				if (!value || 3 != sscanf(value, "%hu %63s %hu%n", &media->ice.remotes[media->ice.remote_count]->component, media->ice.remotes[media->ice.remote_count]->address, &media->ice.remotes[media->ice.remote_count]->port, &n))
					break;

				value += n;
				++media->ice.remote_count;
				media->offset += sizeof(*media->ice.remotes[0]);
			}
		}
		else if (0 == strcmp("setup", name))
		{
			media->setup = sdp_option_setup_from(value);
		}
		else if (0 == strcmp("ssrc", name))
		{
			media->ssrc.ssrc = (uint32_t)strtoul(value, NULL, 10);
			// TODO: ssrc attribute
		}
		else if (0 == strcmp("ssrc-group", name))
		{
			// TODO
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
int rtsp_media_sdp(const char* s, int len, struct rtsp_media_t* medias, int count)
{
	int i, j, n;
	int formats[16];
	const char* control;
	const char* start, *stop;
	const char* iceufrag, *icepwd, *setup;
	const char* username, *session, *version;
	const char* network, *addrtype, *address, *source;
	struct rtsp_media_t* m;
	struct rtsp_header_range_t range;
	sdp_t* sdp;

	sdp = sdp_parse(s, len);
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
		sdp_timing_get(sdp, i, &start, &stop);
	}

	// C.1.7 Connection Information
	network = addrtype = source = NULL;
	if (0 != sdp_connection_get(sdp, &network, &addrtype, &source) || 0 == strcmp("0.0.0.0", source))
		sdp_origin_get(sdp, &username, &session, &version, &network, &addrtype, &source);

	// session ice-ufrag/ice-pwd
	iceufrag = sdp_attribute_find(sdp, "ice-ufrag");
	icepwd = sdp_attribute_find(sdp, "ice-pwd");
	setup = sdp_attribute_find(sdp, "setup");

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
        
		if(0 == sdp_media_get_connection(sdp, i, &network, &addrtype, &address))
        {
			if (0 == strcmp("IP4", addrtype) && 0 == strcmp("0.0.0.0", address) && source && *source)
				address = source;
            snprintf(m->source, sizeof(m->source), "%s", source && *source ? source : "");
            snprintf(m->network, sizeof(m->network), "%s", network);
            snprintf(m->address, sizeof(m->address), "%s", address);
            snprintf(m->addrtype, sizeof(m->addrtype), "%s", addrtype);
        }
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

		// TODO: plan-B streams
		m->mode = sdp_media_mode(sdp, i);
		m->setup = setup ? sdp_option_setup_from(setup) : SDP_A_SETUP_NONE;

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

/// @return -0-no media, >0-ok, <0-error
int rtsp_media_to_sdp(const struct rtsp_media_t* m, char* line, int bytes)
{
	int i, n;
	int setup = 0;
	int port = m->port[0];

	if (SDP_M_PROTO_TEST_TCP(sdp_option_proto_from(m->proto)))
	{
		// try to set tcp active for sender side
		setup = (SDP_A_SETUP_NONE == m->setup || SDP_A_SETUP_ACTPASS == m->setup) ? SDP_A_SETUP_ACTIVE : m->setup;
		//if (SDP_A_SETUP_PASSIVE == setup || SDP_A_SETUP_ACTPASS == setup)
		//	port = options->m[i].port[0];
	}

	n = snprintf(line, bytes, "m=%s %d %s", m->media, port, m->proto);
	for (i = 0; i < m->avformat_count && n < bytes; i++)
	{
		if (m->avformats[i].fmt >= 96 && !m->avformats[i].encoding[0])
			continue; // ignore empty encoding
		n += snprintf(line + n, bytes - n, " %d", m->avformats[i].fmt);
	}
	n += snprintf(line + n, bytes > n ? bytes - n : 0, "\n");

	for (i = 0; i < m->avformat_count && n >= 0 && n < bytes; i++)
	{
		if (!m->avformats[i].encoding[0])
			continue;

		if (SDP_M_MEDIA_VIDEO == sdp_option_media_from(m->media))
		{
			n += snprintf(line + n, bytes - n, "a=rtpmap:%d %s/%d\n", m->avformats[i].fmt, m->avformats[i].encoding, m->avformats[i].rate ? m->avformats[i].rate : 90000);
			if(n >= 0 && n < bytes && m->avformats[i].fmtp && m->avformats[i].fmtp[0])
				n += snprintf(line + n, bytes - n, "a=fmtp:%s\n", m->avformats[i].fmtp);
		}
		else if (SDP_M_MEDIA_AUDIO == sdp_option_media_from(m->media))
		{
			if(m->avformats[i].channel > 0)
				n += snprintf(line + n, bytes - n, "a=rtpmap:%d %s/%d/%d\n", m->avformats[i].fmt, m->avformats[i].encoding, m->avformats[i].rate, m->avformats[i].channel);
			else
				n += snprintf(line + n, bytes - n, "a=rtpmap:%d %s/%d\n", m->avformats[i].fmt, m->avformats[i].encoding, m->avformats[i].rate);

			if (n >= 0 && n < bytes && m->avformats[i].fmtp && m->avformats[i].fmtp[0])
				n += snprintf(line + n, bytes - n, "a=fmtp:%s\n", m->avformats[i].fmtp);
		}
	}

	//for (int j = 0; j < 128 && j < 8 * sizeof(m->payloads) / sizeof(m->payloads[0]); j++)
	//{
	//	if(m->payloads[j/8] & (1<<(j%8)))
	//		n += snprintf(answer+n, sizeof(answer)-n, " %d", j);
	//}

	if (SDP_M_PROTO_TEST_TCP(sdp_option_proto_from(m->proto)))
	{
		n += snprintf(line + n, bytes - n, "a=setup:%s\n", sdp_option_setup_to(setup));
	}

	if (m->nport < 2 || m->port[0] == m->port[1])
	{
		n += snprintf(line + n, bytes - n, "a=rtcp-mux\n");
	}

	n += snprintf(line + n, bytes - n, "a=%s\n", sdp_option_mode_to(m->mode));

	if (m->ssrc.ssrc)
	{
		n += snprintf(line + n, bytes - n, "a=ssrc:%u\n", m->ssrc.ssrc);
	}

	return n > 0 && n < bytes ? n : -1;
}
