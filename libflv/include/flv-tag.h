#ifndef _flv_tag_h_
#define _flv_tag_h_

#include <stdint.h>
#include "flv-proto.h"

struct flv_audio_tag_header_t
{
	uint8_t codecid;	/// audio codec id: FLV_AUDIO_AAC
	uint8_t rate;		/// audio sample frequence: 0-5.5 kHz, 1-11 kHz, 2-22 kHz, 3-44 kHz
	uint8_t bits;		/// audio sample bits: 0-8 bit samples, 1-16-bit samples
	uint8_t channels;	/// audio channel count: 0-Mono sound, 1-Stereo sound
};

struct flv_video_tag_header_t
{
	uint8_t codecid;	/// video codec id: FLV_VIDEO_H264
	uint8_t keyframe;	/// video frame type: 1-key frame, 2-inter frame
};


/// Write flv audio tag header
/// @param[in] audio flv audio parameter
/// @param[in] sequence flv packet type(AAC only): 0-AAC sequence header, 1-AAC raw
/// @return header length in byte
static inline int flv_audio_tag_header(const struct flv_audio_tag_header_t* audio, uint8_t sequence, uint8_t* buf, int len)
{
	if (len < 1 + (FLV_AUDIO_AAC == audio->codecid ? 1 : 0))
		return -1;

	if (FLV_AUDIO_AAC == audio->codecid)
	{
		buf[0] = (FLV_AUDIO_AAC /*<< 4*/) /* SoundFormat */ | (3 << 2) /* 44k-SoundRate */ | (1 << 1) /* 16-bit samples */ | 1 /* Stereo sound */;
		buf[1] = sequence; // AACPacketType
		return 2;
	}
	else
	{
		buf[0] = (audio->codecid /*<< 4*/) | ((audio->rate && 0x03) << 2) | ((audio->bits & 0x01) << 1) | (audio->channels & 0x01);
		return 1;
	}
}

/// Write flv video tag header
/// @param[in] video flv video parameter
/// @param[in] sequence flv packet type(AVC/HEVC only): 0-AVC/HEVC sequence header, 1-AVC/HEVC NALU, 2-AVC/HEVC end of sequence
/// @param[in] cts video composition time(AVC/HEVC only): pts - dts
/// @return header length in byte
static inline int flv_video_tag_header(const struct flv_video_tag_header_t* video, uint8_t sequence, int32_t cts, uint8_t* buf, int len)
{
	if (len < 1)
		return -1;

	buf[0] = (video->keyframe << 4) /*FrameType*/ | (video->codecid & 0x0F) /*CodecID*/;

	if (FLV_VIDEO_H264 == video->codecid || FLV_VIDEO_H265 == video->codecid || FLV_VIDEO_AV1 == video->codecid)
	{
		if (len < 5)
			return -1;

		buf[1] = sequence; // AVCPacketType
		buf[2] = (cts >> 16) & 0xFF;
		buf[3] = (cts >> 8) & 0xFF;
		buf[4] = cts & 0xFF;
		return 5;
	}

	return 1;
}

#endif /* !_flv_tag_h_ */
