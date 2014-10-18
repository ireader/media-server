// RFC-4566 SDP
// 6. SDP Attributes (p30)
// a=fmtp:<format> <format specific parameters>

#include "sdp-a-fmtp.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// RFC6184 RTP Payload Format for H.264 Video
// m=video 49170 RTP/AVP 98
// a=rtpmap:98 H264/90000
// a=fmtp:98 profile-level-id=42A01E;
//			 packetization-mode=1;
//			 sprop-parameter-sets=<parameter sets data>
int sdp_a_fmtp_h264(const char* fmtp, int *format, struct sdp_a_fmtp_h264_t *h264)
{
	size_t n;
	const char *p1, *p2;
	const char *p = fmtp;

	// payload type
	*format = atoi(p);
	p1 = strchr(p, ' ');
	if(' ' != *p1)
		return -1;

	h264->flags = 0;
	assert(' ' == *p1);
	p = p1 + 1;
	while(*p)
	{
		p1 = strchr(p, '=');
		if('=' != *p1)
			return -1;

		p2 = strchr(p1+1, ';');
		if(!p2)
			p2 = p1 + strlen(p1);

		n = p2 - p1 - 1;
		switch(*p)
		{
		case 'p':
			// profile-level-id
			// packetization-mode
			if(0 == strncmp("profile-level-id", p, p1-p) && 6==n)
			{
				assert(6 == n);
				h264->flags |= 1<<SDP_A_FMTP_H264_PROFILE_LEVEL_ID;
				strncpy(h264->profile_level_id, p1+1, 6);
				h264->profile_level_id[6] = '\0';
			}
			else if(0 == strncmp("packetization-mode", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_PACKETIZATION_MODE;
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
			if(0 == strncmp("max-recv-level", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_MAX_RECV_LEVEL;
				h264->max_recv_level = atoi(p1+1);
			}
			else if(0 == strncmp("max-mbps", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_MAX_MBPS;
				h264->max_mbps = atoi(p1+1);
			}
			else if(0 == strncmp("max-smbps", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_MAX_SMBPS;
				h264->max_smbps = atoi(p1+1);
			}
			else if(0 == strncmp("max-fs", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_MAX_FS;
				h264->max_fs = atoi(p1+1);
			}
			else if(0 == strncmp("max-cbp", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_MAX_CPB;
				h264->max_cpb = atoi(p1+1);
			}
			else if(0 == strncmp("max-dbp", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_MAX_DPB;
				h264->max_dpb = atoi(p1+1);
			}
			else if(0 == strncmp("max-br", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_MAX_BR;
				h264->max_br = atoi(p1+1);
			}
			else if(0 == strncmp("max-rcmd-nalu-size", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_MAX_RCMD_NALU_SIZE;
				h264->max_rcmd_nalu_size = atoi(p1+1);
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
			if(0 == strncmp("sprop-parameter-sets", p, p1-p) && n < sizeof(h264->sprop_parameter_sets)-1)
			{
				assert(n < sizeof(h264->sprop_parameter_sets)-1);
				h264->flags |= 1<<SDP_A_FMTP_H264_SPROP_PARAMETER_SETS;
				strncpy(h264->sprop_parameter_sets, p1+1, n);
				h264->sprop_parameter_sets[n] = '\0';
			}
			else if(0 == strncmp("sprop-level-parameter-sets", p, p1-p) && n < sizeof(h264->sprop_level_parameter_sets)-1)
			{
				assert(n < sizeof(h264->sprop_level_parameter_sets)-1);
				h264->flags |= 1<<SDP_A_FMTP_H264_SPROP_LEVEL_PARAMETER_SETS;
				strncpy(h264->sprop_level_parameter_sets, p1+1, n);
				h264->sprop_level_parameter_sets[n] = '\0';
			}
			else if(0 == strncmp("sprop-deint-buf-req", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_SPROP_DEINT_BUF_REQ;
				h264->sprop_deint_buf_req = atoi(p1+1);
			}
			else if(0 == strncmp("sprop-interleaving-depth", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_SPROP_INTERLEAVING_DEPTH;
				h264->sprop_interleaving_depth = atoi(p1+1);
			}
			else if(0 == strncmp("sprop-max-don-diff", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_SPROP_MAX_DON_DIFF;
				h264->sprop_max_don_diff = atoi(p1+1);
			}
			else if(0 == strncmp("sprop-init-buf-time", p, p1-p) && n < sizeof(h264->sprop_init_buf_time)-1)
			{
				assert(n < sizeof(h264->sprop_init_buf_time)-1);
				h264->flags |= 1<<SDP_A_FMTP_H264_SPROP_INIT_BUF_TIME;
				strncpy(h264->sprop_init_buf_time, p1+1, n);
				h264->sprop_init_buf_time[n] = '\0';
			}
			else if(0 == strncmp("sar-understood", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_SAR_UNDERSTOOD;
				h264->sar_understood = atoi(p1+1); 
			}
			else if(0 == strncmp("sar-supported", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_SAR_SUPPORTED;
				h264->sar_supported = atoi(p1+1);
			}
			break;

		case 'r':
			// redundant-pic-cap
			if(0 == strncmp("redundant-pic-cap", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_REDUNDANT_PIC_CAP;
				h264->redundant_pic_cap = atoi(p1+1);
			}
			break;

		case 'd':
			// deint-buf-cap
			if(0 == strncmp("deint-buf-cap", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_DEINT_BUF_CAP;
				h264->deint_buf_cap = atoi(p1+1);
			}
			break;

		case 'i':
			// in-band-parameter-sets
			if(0 == strncmp("in-band-parameter-sets", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_IN_BAND_PARAMETER_SETS;
				h264->in_band_parameter_sets = atoi(p1+1);
			}
			break;

		case 'u':
			// use-level-src-parameter-sets
			if(0 == strncmp("use-level-src-parameter-sets", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_USE_LEVEL_SRC_PARAMETER_SETS;
				h264->use_level_src_parameter_sets = atoi(p1+1);
			}
			break;

		case 'l':
			// level-asymmetry-allowed
			if(0 == strncmp("level-asymmetry-allowed", p, p1-p))
			{
				h264->flags |= 1<<SDP_A_FMTP_H264_LEVEL_ASYMMETRY_ALLOWED;
				h264->level_asymmetry_allowed = atoi(p1+1);
			}
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
// a=fmtp:96 streamtype=3; profile-level-id=1807; mode=generic;
//			 objectType=2; config=0842237F24001FB400094002C0; sizeLength=10;
//			 CTSDeltaLength=16; randomAccessIndication=1; streamStateIndication=4
int sdp_a_fmtp_mpeg4(const char* fmtp, int *format, struct sdp_a_fmtp_mpeg4_t *mpeg4)
{
	size_t n;
	const char *p1, *p2;
	const char *p = fmtp;

	// payload type
	*format = atoi(p);
	p1 = strchr(p, ' ');
	if(' ' != *p1)
		return -1;

	mpeg4->flags = 0;
	assert(' ' == *p1);
	p = p1 + 1;
	while(*p)
	{
		p1 = strchr(p, '=');
		if('=' != *p1)
			return -1;

		p2 = strchr(p1+1, ';');
		if(!p2)
			p2 = p1 + strlen(p1);

		n = p2 - p1 - 1;
		switch(*p)
		{
		case 's':
			// streamType
			// sizeLength
			// streamStateIndication
			if(0 == strncmp("streamType", p, p1-p))
			{
				mpeg4->streamType = atoi(p1+1);
			}
			else if(0 == strncmp("sizeLength", p, p1-p))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_SIZELENGTH;
				mpeg4->sizeLength = atoi(p1+1);
			}
			else if(0 == strncmp("streamStateIndication", p, p1-p))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_STREAMSTATEINDICATION;
				mpeg4->streamStateIndication = atoi(p1+1);
			}
			break;

		case 'p':
			// profile-level-id
			if(0 == strncmp("profile-level-id", p, p1-p) && n < sizeof(mpeg4->profile_level_id)-1)
			{
				strncpy(mpeg4->profile_level_id, p1+1, n);
				mpeg4->profile_level_id[n] = '\0';
			}
			break;

		case 'c':
			// config
			// constantSize
			// constantDuration
			if(0 == strncmp("config", p, p1-p) && n < sizeof(mpeg4->config)-1)
			{
				strncpy(mpeg4->config, p1+1, n);
				mpeg4->config[n] = '\0';
			}
			else if(0 == strncmp("constantSize", p, p1-p))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_CONSTANTSIZE;
				mpeg4->constantSize = atoi(p1+1);
			}
			else if(0 == strncmp("constantDuration", p, p1-p))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_CONSTANTDURATION;
				mpeg4->constantDuration = atoi(p1+1);
			}
			break;

		case 'm':
			// mode
			// maxDisplacement
			if(0 == strncmp("mode", p, p1-p))
			{
				if(0 == strncmp("generic", p1+1, n))
					mpeg4->mode = 1;
				else if(0 == strncmp("CELP-cbr", p1+1, n))
					mpeg4->mode = 2;
				else if(0 == strncmp("CELP-vbr", p1+1, n))
					mpeg4->mode = 3;
				else if(0 == strncmp("AAC-lbr", p1+1, n))
					mpeg4->mode = 4;
				else if(0 == strncmp("AAC-hbr", p1+1, n))
					mpeg4->mode = 5;
				else
					mpeg4->mode = 0; // unknown
			}
			else if(0 == strncmp("maxDisplacement", p, p1-p))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_MAXDISPLACEMENT;
				mpeg4->maxDisplacement = atoi(p1+1);
			}
			break;

		case 'o':
			// objectType
			if(0 == strncmp("objectType", p, p1-p))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_OBJECTTYPE;
				mpeg4->objectType = atoi(p1+1);
			}
			break;

		case 'd':
			// deinterleaveBufferSize
			if(0 == strncmp("deinterleaveBufferSize", p, p1-p))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_DEINTERLEAVEBUFFERSIZE;
				mpeg4->deinterleaveBufferSize = atoi(p1+1);
			}
			break;

		case 'i':
			// indexLength
			// indexDeltaLength
			if(0 == strncmp("indexLength", p, p1-p))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_INDEXLENGTH;
				mpeg4->indexLength = atoi(p1+1);
			}
			else if(0 == strncmp("indexDeltaLength", p, p1-p))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_INDEXDELTALENGTH;
				mpeg4->indexDeltaLength = atoi(p1+1);
			}
			break;

		case 'C':
			// CTSDeltaLength
			if(0 == strncmp("CTSDeltaLength", p, p1-p))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_CTSDELTALENGTH;
				mpeg4->CTSDeltaLength = atoi(p1+1);
			}
			break;

		case 'D':
			// DTSDeltaLength
			if(0 == strncmp("DTSDeltaLength", p, p1-p))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_DTSDELTALENGTH;
				mpeg4->DTSDeltaLength = atoi(p1+1);
			}
			break;

		case 'r':
			// randomAccessIndication
			if(0 == strncmp("randomAccessIndication", p, p1-p))
			{
				mpeg4->flags |= SDP_A_FMTP_MPEG4_RANDOMACCESSINDICATION;
				mpeg4->randomAccessIndication = atoi(p1+1);
			}
			break;

		case 'a':
			// auxiliaryDataSizeLength
			if(0 == strncmp("auxiliaryDataSizeLength", p, p1-p))
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
