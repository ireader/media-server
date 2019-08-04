#ifndef _avpacket_h_
#define _avpacket_h_

#include <stdint.h>

enum AVPACKET_CODEC_ID
{
	AVCODEC_NONE = 0x00,

	AVCODEC_VIDEO_MPEG2,
	AVCODEC_VIDEO_MPEG4,
	AVCODEC_VIDEO_H264,
	AVCODEC_VIDEO_H265,
	AVCODEC_VIDEO_VP8,
	AVCODEC_VIDEO_VP9,

	AVCODEC_IMAGE_PNG = 0x100,
	AVCODEC_IMAGE_GIF,
	AVCODEC_IMAGE_BMP,
	AVCODEC_IMAGE_JPEG,

	AVCODEC_AUDIO_PCM = 0x10000,
	AVCODEC_AUDIO_G711,
	AVCODEC_AUDIO_G726,
	AVCODEC_AUDIO_G729,
	AVCODEC_AUDIO_MP3,
	AVCODEC_AUDIO_AAC,
	AVCODEC_AUDIO_AC3,
	AVCODEC_AUDIO_OPUS,
    AVCODEC_AUDIO_MP2, // MPEG-2 Layer II
};

#define AVPACKET_FLAG_KEY 0x01

struct avpacket_t
{
	int stream;
	int size;
	uint8_t* data;

	int64_t pts;
	int64_t dts;

	enum AVPACKET_CODEC_ID codecid;
	int flags; // AVPACKET_FLAG_XXX

	void* opaque; // internal use only
};

#ifdef __cplusplus
extern "C" {
#endif

///@param[in] size alloc packet data size, don't include sizeof(struct avpacket_t)
///@return alloc new avpacket_t, use avpacket_release to free memory
struct avpacket_t* avpacket_alloc(int size);
int32_t avpacket_addref(struct avpacket_t* pkt);
int32_t avpacket_release(struct avpacket_t* pkt);

#ifdef __cplusplus
}
#endif
#endif /* !_avpacket_h_ */
