#ifndef _flv_proto_h_
#define _flv_proto_h_

// FLV Tag Type
#define FLV_TYPE_AUDIO		8
#define FLV_TYPE_VIDEO		9
#define FLV_TYPE_SCRIPT		18

// FLV Audio Type
#define FLV_AUDIO_ADPCM		(1 << 4)
#define FLV_AUDIO_MP3		(2 << 4)
#define FLV_AUDIO_G711		(7 << 4)
#define FLV_AUDIO_AAC		(10 << 4)
#define FLV_AUDIO_MP3_8K	(14 << 4)
#define FLV_AUDIO_ASC		0x100 // AudioSpecificConfig(ISO-14496-3)

// FLV Video Type
#define FLV_VIDEO_H263		2
#define FLV_VIDEO_VP6		4
#define FLV_VIDEO_H264		7
#define FLV_VIDEO_H265		12 // https://github.com/CDN-Union/H265
#define FLV_VIDEO_AVCC		0x200 // AVCDecoderConfigurationRecord(ISO-14496-15)
#define FLV_VIDEO_HVCC		0x201 // HEVCDecoderConfigurationRecord(ISO-14496-15)

#endif /* !_flv_proto_h_ */
