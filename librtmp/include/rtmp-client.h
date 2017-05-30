#ifndef _rtmp_client_h_
#define _rtmp_client_h_

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct rtmp_client_handler_t
{
	///network implementation
	///@return >0-sent bytes, <0-error
	int (*send)(void* param, const void* header, size_t len, const void* payload, size_t bytes);

	///VOD only
	///@param[in] video FLV VideoTagHeader + AVCVIDEOPACKET: AVCDecoderConfigurationRecord(ISO 14496-15) / One or more NALUs(four-bytes length + NALU)
	///@param[in] audio FLV AudioTagHeader + AACAUDIODATA: AudioSpecificConfig(14496-3) / Raw AAC frame data in UI8
	///@param[in] meta AMF0/AMF3
	///@return 0-ok, other-error
	int (*onvideo)(void* param, const void* video, size_t bytes, uint32_t timestamp);
	int (*onaudio)(void* param, const void* audio, size_t bytes, uint32_t timestamp);
	int (*onmeta)(void* param, const void* meta, size_t bytes);

	///VOD only
	///user alloc video/audio memory
	///@param[in] avtype 0-audio, 1-video
	///@param[in] bytes video/audio buffer size in byte
	///@return NULL-failed, other-memory pointer
	void* (*alloc)(void* param, int avtype, size_t bytes);
};

/// setup URL and connect server(timeout???)
void* rtmp_client_create(const char* appname, const char* playpath, const char* tcurl, void* param, const struct rtmp_client_handler_t* handler);
void rtmp_client_destroy(void* client);

///@return 0-ok, other-error
int rtmp_client_input(void* client, const void* data, size_t bytes);

///@param[in] publish, 0-Publish(push stream to server), 1-LIVE/VOD(pull from server), 2-LIVE only, 3-VOD only
///@return 0-ok, other-error
int rtmp_client_start(void* client, int publish);
int rtmp_client_stop(void* client);
int rtmp_client_pause(void* client, int pause); // VOD only
int rtmp_client_seek(void* client, double timestamp); // VOD only

///@return RTMP_STATE_START(4): push video/audio
int rtmp_client_getstate(void* client);

///@param[in] data FLV VideoTagHeader + AVCVIDEOPACKET: AVCDecoderConfigurationRecord(ISO 14496-15) / One or more NALUs(four-bytes length + NALU)
///@param[in] bytes: video data length in bytes
///@return 0-ok, other-error
int rtmp_client_push_video(void* client, const void* video, size_t bytes, uint32_t timestamp);

///@param[in] data FLV AudioTagHeader + AACAUDIODATA: AudioSpecificConfig(14496-3) / Raw AAC frame data in UI8
///@param[in] bytes: audio data length in bytes
///@return 0-ok, other-error
int rtmp_client_push_audio(void* client, const void* audio, size_t bytes, uint32_t timestamp);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtmp_client_h_ */
