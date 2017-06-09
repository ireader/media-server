#ifndef _flv_proto_h_
#define _flv_proto_h_

// FLV Tag Type
#define FLV_TYPE_AUDIO		8
#define FLV_TYPE_VIDEO		9
#define FLV_TYPE_SCRIPT		18

// FLV Audio Type
#define FLV_AUDIO_ADPCM		1
#define FLV_AUDIO_MP3		2
#define FLV_AUDIO_G711		7
#define FLV_AUDIO_AAC		10
#define FLV_AUDIO_MP3_8K	14
#define FLV_AUDIO_ASC		16 // AudioSpecificConfig(ISO-14496-3)

// FLV Video Type
#define FLV_VIDEO_H263		2
#define FLV_VIDEO_VP6		4
#define FLV_VIDEO_H264		7
#define FLV_VIDEO_AVCC		16 // AVCDecoderConfigurationRecord(ISO-14496-15)

#endif /* !_flv_proto_h_ */
