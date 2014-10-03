#ifndef _sdp_a_fmtp_h_
#define _sdp_a_fmtp_h_

// H.264
enum { 
	SDP_A_FMTP_H264_PROFILE_LEVEL_ID = 0,
	SDP_A_FMTP_H264_MAX_RECV_LEVEL,
	SDP_A_FMTP_H264_MAX_MBPS,
	SDP_A_FMTP_H264_MAX_SMBPS,
	SDP_A_FMTP_H264_MAX_FS,
	SDP_A_FMTP_H264_MAX_CPB,
	SDP_A_FMTP_H264_MAX_DPB,
	SDP_A_FMTP_H264_MAX_BR,
	SDP_A_FMTP_H264_REDUNDANT_PIC_CAP,
	SDP_A_FMTP_H264_SPROP_PARAMETER_SETS,
	SDP_A_FMTP_H264_SPROP_LEVEL_PARAMETER_SETS,
	SDP_A_FMTP_H264_USE_LEVEL_SRC_PARAMETER_SETS,
	SDP_A_FMTP_H264_IN_BAND_PARAMETER_SETS,
	SDP_A_FMTP_H264_LEVEL_ASYMMETRY_ALLOWED,
	SDP_A_FMTP_H264_PACKETIZATION_MODE,
	SDP_A_FMTP_H264_SPROP_INTERLEAVING_DEPTH,
	SDP_A_FMTP_H264_SPROP_DEINT_BUF_REQ,
	SDP_A_FMTP_H264_DEINT_BUF_CAP,
	SDP_A_FMTP_H264_SPROP_INIT_BUF_TIME,
	SDP_A_FMTP_H264_SPROP_MAX_DON_DIFF,
	SDP_A_FMTP_H264_MAX_RCMD_NALU_SIZE,
	SDP_A_FMTP_H264_SAR_UNDERSTOOD,
	SDP_A_FMTP_H264_SAR_SUPPORTED,
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
	int packetization_mode; // (p51)
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


// mpeg4-generic
enum { 
	SDP_A_FMTP_MPEG4_OBJECTTYPE = 0,
	SDP_A_FMTP_MPEG4_CONSTANTSIZE,
	SDP_A_FMTP_MPEG4_CONSTANTDURATION,
	SDP_A_FMTP_MPEG4_MAXDISPLACEMENT,
	SDP_A_FMTP_MPEG4_DEINTERLEAVEBUFFERSIZE,
	SDP_A_FMTP_MPEG4_SIZELENGTH,
	SDP_A_FMTP_MPEG4_INDEXLENGTH,
	SDP_A_FMTP_MPEG4_INDEXDELTALENGTH,
	SDP_A_FMTP_MPEG4_CTSDELTALENGTH,
	SDP_A_FMTP_MPEG4_DTSDELTALENGTH,
	SDP_A_FMTP_MPEG4_RANDOMACCESSINDICATION,
	SDP_A_FMTP_MPEG4_STREAMSTATEINDICATION,
	SDP_A_FMTP_MPEG4_AUXILIARYDATASIZELENGTH,
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
