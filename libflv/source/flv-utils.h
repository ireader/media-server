#ifndef _flv_common_h_
#define _flv_common_h_

// FLV Tag Type
#define FLV_TYPE_AUDIO		8
#define FLV_TYPE_VIDEO		9
#define FLV_TYPE_SCRIPT		18

// FLV Audio Type
#define FLV_AUDIO_ADPCM		1
#define FLV_AUDIO_MP3		2
#define FLV_AUDIO_G711		7
#define FLV_AUDIO_AAC		10
#define FLV_AUDIO_MP3_8k	14

// FLV Video Type
#define FLV_VIDEO_H263		2
#define FLV_VIDEO_VP6		4
#define FLV_VIDEO_AVC		7

#define H264_NAL_IDR		5 // Coded slice of an IDR picture
#define H264_NAL_SPS		7 // Sequence parameter set
#define H264_NAL_PPS		8 // Picture parameter set

#endif /* !_flv_common_h_ */
