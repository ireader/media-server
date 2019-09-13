// RFC-4566 SDP
// 6. SDP Attributes (p30)
// a=fmtp:<format> <format specific parameters>

#include "sdp-a-fmtp.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(OS_WINDOWS)
#define strncasecmp _strnicmp
#endif

// RFC6184 RTP Payload Format for H.264 Video
// m=video 49170 RTP/AVP 98
// a=rtpmap:98 H264/90000
// a=fmtp:98 profile-level-id=42A01E;
//			 packetization-mode=1;
//			 sprop-parameter-sets=<parameter sets data>
int sdp_a_fmtp_h264(const char* fmtp, int *format, struct sdp_a_fmtp_h264_t *h264)
{
	size_t nc, vc;
	const char *p1, *p2;
	const char *p = fmtp;

	// payload type
	*format = atoi(p);
	p1 = strchr(p, ' ');
	if(!p1 || ' ' != *p1)
		return -1;

	h264->flags = 0;
	assert(' ' == *p1);
	p = p1 + 1;
	while(*p)
	{
		p1 = strchr(p, '=');
		if(!p1 || '=' != *p1)
			return -1;

		p2 = strchr(p1+1, ';');
		if(!p2)
			p2 = p1 + strlen(p1);

		while(' ' == *p) p++; // skip space

        nc = (size_t)(p1 - p); // ptrdiff_t to size_t
		vc = (size_t)(p2 - p1 - 1); // ptrdiff_t to size_t
		switch(*p)
		{
		case 'p':
			// profile-level-id
			// packetization-mode
			if(0 == strncasecmp("profile-level-id", p, nc))
			{
				if(6 != vc) return -1;
				h264->flags |= SDP_A_FMTP_H264_PROFILE_LEVEL_ID;
				memcpy(h264->profile_level_id, p1+1, 6);
				h264->profile_level_id[6] = '\0';
			}
			else if(0 == strncasecmp("packetization-mode", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_PACKETIZATION_MODE;
				h264->packetization_mode = atoi(p1+1);
			}
			break;

		case 'm':
			// max-recv-level
			// max-mbps
			// max-smbps
			// max-fs
			// max-cbp
			// max-dbp
			// max-br
			// max-rcmd-nalu-size
			if(0 == strncasecmp("max-recv-level", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_MAX_RECV_LEVEL;
				h264->max_recv_level = atoi(p1+1);
			}
			else if(0 == strncasecmp("max-mbps", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_MAX_MBPS;
				h264->max_mbps = atoi(p1+1);
			}
			else if(0 == strncasecmp("max-smbps", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_MAX_SMBPS;
				h264->max_smbps = atoi(p1+1);
			}
			else if(0 == strncasecmp("max-fs", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_MAX_FS;
				h264->max_fs = atoi(p1+1);
			}
			else if(0 == strncasecmp("max-cbp", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_MAX_CPB;
				h264->max_cpb = atoi(p1+1);
			}
			else if(0 == strncasecmp("max-dbp", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_MAX_DPB;
				h264->max_dpb = atoi(p1+1);
			}
			else if(0 == strncasecmp("max-br", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_MAX_BR;
				h264->max_br = atoi(p1+1);
			}
			else if(0 == strncasecmp("max-rcmd-nalu-size", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_MAX_RCMD_NALU_SIZE;
				h264->max_rcmd_nalu_size = (unsigned int)atoi(p1+1);
			}
			break;

		case 's':
			// sprop-parameter-sets
			// sprop-level-parameter-sets
			// sprop-deint-buf-req
			// sprop-interleaving-depth
			// sprop-max-don-diff
			// sprop-init-buf-time
			// sar-understood
			// sar-supported
			if(0 == strncasecmp("sprop-parameter-sets", p, nc))
			{
				if(vc >= sizeof(h264->sprop_parameter_sets)) return -1;
				h264->flags |= SDP_A_FMTP_H264_SPROP_PARAMETER_SETS;
				memcpy(h264->sprop_parameter_sets, p1+1, vc);
				h264->sprop_parameter_sets[vc] = '\0';
			}
			else if(0 == strncasecmp("sprop-level-parameter-sets", p, nc))
			{
				if(vc >= sizeof(h264->sprop_level_parameter_sets)) return -1;
				h264->flags |= SDP_A_FMTP_H264_SPROP_LEVEL_PARAMETER_SETS;
				memcpy(h264->sprop_level_parameter_sets, p1+1, vc);
				h264->sprop_level_parameter_sets[vc] = '\0';
			}
			else if(0 == strncasecmp("sprop-deint-buf-req", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_SPROP_DEINT_BUF_REQ;
				h264->sprop_deint_buf_req = (unsigned int)atoi(p1+1);
			}
			else if(0 == strncasecmp("sprop-interleaving-depth", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_SPROP_INTERLEAVING_DEPTH;
				h264->sprop_interleaving_depth = atoi(p1+1);
			}
			else if(0 == strncasecmp("sprop-max-don-diff", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_SPROP_MAX_DON_DIFF;
				h264->sprop_max_don_diff = (unsigned int)atoi(p1+1);
			}
			else if(0 == strncasecmp("sprop-init-buf-time", p, nc))
			{
				if(vc >= sizeof(h264->sprop_init_buf_time)) return -1;
				h264->flags |= SDP_A_FMTP_H264_SPROP_INIT_BUF_TIME;
				memcpy(h264->sprop_init_buf_time, p1+1, vc);
				h264->sprop_init_buf_time[vc] = '\0';
			}
			else if(0 == strncasecmp("sar-understood", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_SAR_UNDERSTOOD;
				h264->sar_understood = atoi(p1+1); 
			}
			else if(0 == strncasecmp("sar-supported", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_SAR_SUPPORTED;
				h264->sar_supported = atoi(p1+1);
			}
			break;

		case 'r':
			// redundant-pic-cap
			if(0 == strncasecmp("redundant-pic-cap", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_REDUNDANT_PIC_CAP;
				h264->redundant_pic_cap = atoi(p1+1);
			}
			break;

		case 'd':
			// deint-buf-cap
			if(0 == strncasecmp("deint-buf-cap", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_DEINT_BUF_CAP;
				h264->deint_buf_cap = (unsigned int)atoi(p1+1);
			}
			break;

		case 'i':
			// in-band-parameter-sets
			if(0 == strncasecmp("in-band-parameter-sets", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_IN_BAND_PARAMETER_SETS;
				h264->in_band_parameter_sets = atoi(p1+1);
			}
			break;

		case 'u':
			// use-level-src-parameter-sets
			if(0 == strncasecmp("use-level-src-parameter-sets", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_USE_LEVEL_SRC_PARAMETER_SETS;
				h264->use_level_src_parameter_sets = atoi(p1+1);
			}
			break;

		case 'l':
			// level-asymmetry-allowed
			if(0 == strncasecmp("level-asymmetry-allowed", p, nc))
			{
				h264->flags |= SDP_A_FMTP_H264_LEVEL_ASYMMETRY_ALLOWED;
				h264->level_asymmetry_allowed = atoi(p1+1);
			}
			break;
		}

		p = *p2 ? p2 + 1 : p2;
	}

	return 0;
}

// RFC7798 RTP Payload Format for High Efficiency Video Coding (HEVC)
// m=video 49170 RTP/AVP 98
// a=rtpmap:98 H265/90000
// a=fmtp:98 profile-id=1; sprop-vps=<video parameter sets data>
int sdp_a_fmtp_h265(const char* fmtp, int *format, struct sdp_a_fmtp_h265_t *h265)
{
	size_t nc, vc;
	const char *p1, *p2;
	const char *p = fmtp;

	// payload type
	*format = atoi(p);
	p1 = strchr(p, ' ');
	if (!p1 || ' ' != *p1)
		return -1;

	h265->flags = 0;
	assert(' ' == *p1);
	p = p1 + 1;
	while (*p)
	{
		p1 = strchr(p, '=');
		if (!p1 || '=' != *p1)
			return -1;

		p2 = strchr(p1 + 1, ';');
		if (!p2)
			p2 = p1 + strlen(p1);

		while (' ' == *p) p++; // skip space

		nc = (size_t)(p1 - p); // ptrdiff_t to size_t
		vc = (size_t)(p2 - p1 - 1); // ptrdiff_t to size_t
		switch (*p)
		{
		case 'i':
			// interop-constraints
			break;
		case 'l':
			// level-id
			break;
		case 'p':
			// profile-space
			// profile-id
			// profile-compatibility-indicator
			break;

		case 's':
			// sprop-vps
			// sprop-sps
			// sprop-pps
			// sprop-sei
			if (0 == strncasecmp("sprop-vps", p, nc))
			{
			}
			else if (0 == strncasecmp("sprop-sps", p, nc))
			{
			}
			else if (0 == strncasecmp("sprop-pps", p, nc))
			{
			}
			else if (0 == strncasecmp("sprop-sei", p, nc))
			{
			}
			break;

		case 't':
			// tier-flag
			break;
		}

		p = *p2 ? p2 + 1 : p2;
	}

	return 0;
}

// RFC3640 RTP Payload Format for Transport of MPEG-4 Elementary Streams
// m=audio 49230 RTP/AVP 96
// a=rtpmap:96 mpeg4-generic/16000/1
// a=fmtp:96 streamtype=5; profile-level-id=14; mode=CELP-cbr; config=440E00; constantSize=27; constantDuration=240
//
// m=video 49230 RTP/AVP 96
// a=rtpmap:96 mpeg4-generic/1000
// a=fmtp:96 streamType=3; profile-level-id=1807; mode=generic;
//			 objectType=2; config=0842237F24001FB400094002C0; sizeLength=10;
//			 CTSDeltaLength=16; randomAccessIndication=1; streamStateIndication=4
int sdp_a_fmtp_mpeg4(const char* fmtp, int *format, struct sdp_a_fmtp_mpeg4_t *mpeg4)
{
	size_t nc, vc;
	const char *p1, *p2;
	const char *p = fmtp;

	// payload type
	*format = atoi(p);
	p1 = strchr(p, ' ');
	if(!p1 || ' ' != *p1)
		return -1;

	mpeg4->flags = 0;
	assert(' ' == *p1);
	p = p1 + 1;
	while(*p)
	{
		p1 = strchr(p, '=');
		if(!p1 || '=' != *p1)
			return -1;

		p2 = strchr(p1+1, ';');
		if(!p2)
			p2 = p1 + strlen(p1);

		while(' ' == *p) p++; // skip space

        nc = (size_t)(p1 - p); // ptrdiff_t to size_t
		vc = (size_t)(p2 - p1 - 1); // ptrdiff_t to size_t
		switch(*p)
		{
		case 's':
			// streamType
			// sizeLength
			// streamStateIndication
			if(0 == strncasecmp("streamType", p, nc))
			{
				mpeg4->streamType = atoi(p1+1);
			}
			else if(0 == strncasecmp("sizeLength", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_SIZELENGTH;
				mpeg4->sizeLength = atoi(p1+1);
			}
			else if(0 == strncasecmp("streamStateIndication", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_STREAMSTATEINDICATION;
				mpeg4->streamStateIndication = atoi(p1+1);
			}
			break;

		case 'p':
			// profile-level-id
			if(0 == strncasecmp("profile-level-id", p, nc))
			{
                if(vc >= sizeof(mpeg4->profile_level_id)) return -1;
				memcpy(mpeg4->profile_level_id, p1+1, vc);
				mpeg4->profile_level_id[vc] = '\0';
			}
			break;

		case 'c':
			// config
			// constantSize
			// constantDuration
			if(0 == strncasecmp("config", p, nc))
			{
                if(vc >= sizeof(mpeg4->config)) return -1;
				memcpy(mpeg4->config, p1+1, vc);
				mpeg4->config[vc] = '\0';
			}
			else if(0 == strncasecmp("constantSize", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_CONSTANTSIZE;
				mpeg4->constantSize = atoi(p1+1);
			}
			else if(0 == strncasecmp("constantDuration", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_CONSTANTDURATION;
				mpeg4->constantDuration = atoi(p1+1);
			}
			break;

		case 'm':
			// mode
			// maxDisplacement
			if(0 == strncasecmp("mode", p, nc))
			{
				if(0 == strncasecmp("generic", p1+1, vc))
					mpeg4->mode = 1;
				else if(0 == strncasecmp("CELP-cbr", p1+1, vc))
					mpeg4->mode = 2;
				else if(0 == strncasecmp("CELP-vbr", p1+1, vc))
					mpeg4->mode = 3;
				else if(0 == strncasecmp("AAC-lbr", p1+1, vc))
					mpeg4->mode = 4;
				else if(0 == strncasecmp("AAC-hbr", p1+1, vc))
					mpeg4->mode = 5;
				else
					mpeg4->mode = 0; // unknown
			}
			else if(0 == strncasecmp("maxDisplacement", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_MAXDISPLACEMENT;
				mpeg4->maxDisplacement = atoi(p1+1);
			}
			break;

		case 'o':
			// objectType
			if(0 == strncasecmp("objectType", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_OBJECTTYPE;
				mpeg4->objectType = atoi(p1+1);
			}
			break;

		case 'd':
			// deinterleaveBufferSize
			if(0 == strncasecmp("deinterleaveBufferSize", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_DEINTERLEAVEBUFFERSIZE;
				mpeg4->deinterleaveBufferSize = atoi(p1+1);
			}
			break;

		case 'i':
			// indexLength
			// indexDeltaLength
			if(0 == strncasecmp("indexLength", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_INDEXLENGTH;
				mpeg4->indexLength = atoi(p1+1);
			}
			else if(0 == strncasecmp("indexDeltaLength", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_INDEXDELTALENGTH;
				mpeg4->indexDeltaLength = atoi(p1+1);
			}
			break;

		case 'C':
			// CTSDeltaLength
			if(0 == strncasecmp("CTSDeltaLength", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_CTSDELTALENGTH;
				mpeg4->CTSDeltaLength = atoi(p1+1);
			}
			break;

		case 'D':
			// DTSDeltaLength
			if(0 == strncasecmp("DTSDeltaLength", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_DTSDELTALENGTH;
				mpeg4->DTSDeltaLength = atoi(p1+1);
			}
			break;

		case 'r':
			// randomAccessIndication
			if(0 == strncasecmp("randomAccessIndication", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_RANDOMACCESSINDICATION;
				mpeg4->randomAccessIndication = atoi(p1+1);
			}
			break;

		case 'a':
			// auxiliaryDataSizeLength
			if(0 == strncasecmp("auxiliaryDataSizeLength", p, nc))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_AUXILIARYDATASIZELENGTH;
				mpeg4->auxiliaryDataSizeLength = atoi(p1+1);
			}
			break;
		}

		p = *p2 ? p2 + 1 : p2;
	}

	return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
static void sdp_a_fmtp_h264_test(void)
{
	int format = 0;
	struct sdp_a_fmtp_h264_t h264;
	const char* fmtp1 = "98  profile-level-id=42A01E;packetization-mode=1; sprop-parameter-sets=abcd";

	assert(0 == sdp_a_fmtp_h264(fmtp1, &format, &h264));
	assert(98 == format);
	assert(h264.flags == (SDP_A_FMTP_H264_PROFILE_LEVEL_ID|SDP_A_FMTP_H264_PACKETIZATION_MODE|SDP_A_FMTP_H264_SPROP_PARAMETER_SETS));
	assert(0 == strcmp("42A01E", h264.profile_level_id));
	assert(1 == h264.packetization_mode);
	assert(0 == strcmp("abcd", h264.sprop_parameter_sets));
}

static void sdp_a_fmtp_mpeg4_test(void)
{
	int format = 0;
	struct sdp_a_fmtp_mpeg4_t mpeg4;
	const char* fmtp1 = "96 streamType=3;profile-level-id=1807; mode=generic;objectType=2; config=0842237F24001FB400094002C0;sizeLength=10;CTSDeltaLength=16;randomAccessIndication=1;streamStateIndication=4";

	assert(0 == sdp_a_fmtp_mpeg4(fmtp1, &format, &mpeg4));
	assert(96 == format);
	assert(mpeg4.flags == (SDP_A_FMTP_MPEG4_OBJECTTYPE|SDP_A_FMTP_MPEG4_SIZELENGTH|SDP_A_FMTP_MPEG4_CTSDELTALENGTH|SDP_A_FMTP_MPEG4_RANDOMACCESSINDICATION|SDP_A_FMTP_MPEG4_STREAMSTATEINDICATION));
	assert(3 == mpeg4.streamType);
	assert(0 == strcmp("1807", mpeg4.profile_level_id));
	assert(1 == mpeg4.mode);
	assert(2 == mpeg4.objectType);
	assert(0 == strcmp(mpeg4.config, "0842237F24001FB400094002C0"));
	assert(10 == mpeg4.sizeLength);
	assert(16 == mpeg4.CTSDeltaLength);
	assert(1 == mpeg4.randomAccessIndication);
	assert(4 == mpeg4.streamStateIndication);
}

void sdp_a_fmtp_test(void)
{
	sdp_a_fmtp_h264_test();
	sdp_a_fmtp_mpeg4_test();
}
#endif
