#ifndef _sdp_a_fmtp_h_
#define _sdp_a_fmtp_h_

// H.264
enum { 
	SDP_A_FMTP_H264_PROFILE_LEVEL_ID			= 0x00000001,
	SDP_A_FMTP_H264_MAX_RECV_LEVEL				= 0x00000002,
	SDP_A_FMTP_H264_MAX_MBPS					= 0x00000004,
	SDP_A_FMTP_H264_MAX_SMBPS					= 0x00000008,
	SDP_A_FMTP_H264_MAX_FS						= 0x00000010,
	SDP_A_FMTP_H264_MAX_CPB						= 0x00000020,
	SDP_A_FMTP_H264_MAX_DPB						= 0x00000040,
	SDP_A_FMTP_H264_MAX_BR						= 0x00000080,
	SDP_A_FMTP_H264_REDUNDANT_PIC_CAP			= 0x00000100,
	SDP_A_FMTP_H264_SPROP_PARAMETER_SETS		= 0x00000200,
	SDP_A_FMTP_H264_SPROP_LEVEL_PARAMETER_SETS	= 0x00000400,
	SDP_A_FMTP_H264_USE_LEVEL_SRC_PARAMETER_SETS= 0x00000800,
	SDP_A_FMTP_H264_IN_BAND_PARAMETER_SETS		= 0x00001000,
	SDP_A_FMTP_H264_LEVEL_ASYMMETRY_ALLOWED		= 0x00002000,
	SDP_A_FMTP_H264_PACKETIZATION_MODE			= 0x00004000,
	SDP_A_FMTP_H264_SPROP_INTERLEAVING_DEPTH	= 0x00008000,
	SDP_A_FMTP_H264_SPROP_DEINT_BUF_REQ			= 0x00010000,
	SDP_A_FMTP_H264_DEINT_BUF_CAP				= 0x00020000,
	SDP_A_FMTP_H264_SPROP_INIT_BUF_TIME			= 0x00040000,
	SDP_A_FMTP_H264_SPROP_MAX_DON_DIFF			= 0x00080000,
	SDP_A_FMTP_H264_MAX_RCMD_NALU_SIZE			= 0x00100000,
	SDP_A_FMTP_H264_SAR_UNDERSTOOD				= 0x00200000,
	SDP_A_FMTP_H264_SAR_SUPPORTED				= 0x00400000,
};

struct sdp_a_fmtp_h264_t
{
	int flags; // test with (1<<SDP_A_FMTP_H264_xxx)
	char profile_level_id[7]; // (p39): profile_idc/profile_iop/level_idc 
	int max_recv_level; // (p42): profile_iop/level_idc
	int max_mbps; // (p43): maximum macroblock processing rate in units of macroblocks per second
	int max_smbps;// (p44)
	int max_fs; // (p44): the maximum frame size in units of macroblocks.
	int max_cpb; // (p45): the maximum coded picture buffer size
	int max_dpb; // (p46): the maximum decoded picture buffer size in units of 8/3 macroblocks
	int max_br; // (p47): the maximum video bitrate
	int redundant_pic_cap; // (p48): 0-no redundant coded picture, 1-other
	char sprop_parameter_sets[512]; // (p48)
	char sprop_level_parameter_sets[128];
	int use_level_src_parameter_sets; // (p50) value: 0/1 only
	int in_band_parameter_sets; // (p50) value: 0/1 only
	int level_asymmetry_allowed; // (p51) 
	int packetization_mode; // (p51) 0-single NAL mode, 1-non-interleaved mode, 2-interleaved mode
	int sprop_interleaving_depth; // (p51)
	unsigned int sprop_deint_buf_req; // (p52) value: [0,4294967295]
	unsigned int deint_buf_cap; // (p52) value: [0,4294967295]
	char sprop_init_buf_time[512]; // (p53)
	unsigned int sprop_max_don_diff; // (p54) value: [0,32767]
	unsigned int max_rcmd_nalu_size; // (p55) value: [0,4294967295]
	int sar_understood;
	int sar_supported;
};

int sdp_a_fmtp_h264(const char* fmtp, int *format, struct sdp_a_fmtp_h264_t *h264);

// H.265
enum {
	SDP_A_FMTP_H265_SPROP_VPS = 0x00000100,
	SDP_A_FMTP_H265_SPROP_SPS = 0x00000200,
	SDP_A_FMTP_H265_SPROP_PPS = 0x00000400,
	SDP_A_FMTP_H265_SPROP_SEI = 0x00000800,
};
struct sdp_a_fmtp_h265_t
{
	int flags; // test with (1<<SDP_A_FMTP_H265_xxx)
	char sprop_vps[1];
	char sprop_sps[1];
	char sprop_pps[1];
};

int sdp_a_fmtp_h265(const char* fmtp, int *format, struct sdp_a_fmtp_h265_t *h265);

// mpeg4-generic
enum { 
	SDP_A_FMTP_MPEG4_OBJECTTYPE				= 0x0001,
	SDP_A_FMTP_MPEG4_CONSTANTSIZE			= 0x0002,
	SDP_A_FMTP_MPEG4_CONSTANTDURATION		= 0x0004,
	SDP_A_FMTP_MPEG4_MAXDISPLACEMENT		= 0x0008,
	SDP_A_FMTP_MPEG4_DEINTERLEAVEBUFFERSIZE	= 0x0010,
	SDP_A_FMTP_MPEG4_SIZELENGTH				= 0x0020,
	SDP_A_FMTP_MPEG4_INDEXLENGTH			= 0x0040,
	SDP_A_FMTP_MPEG4_INDEXDELTALENGTH		= 0x0080,
	SDP_A_FMTP_MPEG4_CTSDELTALENGTH			= 0x0100,
	SDP_A_FMTP_MPEG4_DTSDELTALENGTH			= 0x0200,
	SDP_A_FMTP_MPEG4_RANDOMACCESSINDICATION	= 0x0400,
	SDP_A_FMTP_MPEG4_STREAMSTATEINDICATION	= 0x0800,
	SDP_A_FMTP_MPEG4_AUXILIARYDATASIZELENGTH= 0x1000,
};

struct sdp_a_fmtp_mpeg4_t
{
	int flags; // test with (1<<SDP_A_FMTP_MPEG4_xxx)

	// required(p28)
	int streamType;
	char profile_level_id[7];
	char config[512];
	int mode; // value: generic/CELP-cbr/CELP-vbr/AAC-lbr/AAC-hbr

	// optional general parameters(p30)
	int objectType;
	int constantSize;
	int constantDuration;
	int maxDisplacement;
	int deinterleaveBufferSize;

	// Optional configuration parameters(p31)
	int sizeLength;
	int indexLength;
	int indexDeltaLength;
	int CTSDeltaLength;
	int DTSDeltaLength;
	int randomAccessIndication;
	int streamStateIndication;
	int auxiliaryDataSizeLength;
};

int sdp_a_fmtp_mpeg4(const char* fmtp, int *format, struct sdp_a_fmtp_mpeg4_t *mpeg4);

#endif /* !_sdp_a_fmtp_h_ */
