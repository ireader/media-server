#ifndef _rtmp_client_h_
#define _rtmp_client_h_

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct rtmp_client_handler_t
{
	/// network implemention
	/// @return >0-sent bytes, <0-error
	int (*send)(void* param, const void* data, size_t bytes);

	void (*onerror)(void* param, int code, const char* msg);

	// VOD only
	///@param[in] data FLV VideoTagHeader + AVCVIDEOPACKET: AVCDecoderConfigurationRecord(ISO 14496-15) / One or more NALUs(four-bytes length + NALU)
	void (*onvideo)(void* param, const void* data, size_t bytes, uint32_t timestamp);
	///@param[in] data FLV AudioTagHeader + AACAUDIODATA: AudioSpecificConfig(14496-3) / Raw AAC frame data in UI8
	void (*onaudio)(void* param, const void* data, size_t bytes, uint32_t timestamp);
	void (*onmeta)(void* param, const void* data, size_t bytes);
};

/// setup URL and connect server(timeout???)
void* rtmp_client_create(const char* appname, const char* playpath, const char* tcurl, void* param, const struct rtmp_client_handler_t* handler);
void rtmp_client_destroy(void* client);

int rtmp_client_input(void* client, const void* data, size_t bytes);

///@param[in] publish, 0-Publish(push stream to server), 1-LIVE/VOD(pull from server), 2-LIVE only, 3-VOD only
int rtmp_client_start(void* client, int publish);
int rtmp_client_stop(void* client);
int rtmp_client_pause(void* client, int pause); // VOD only
int rtmp_client_seek(void* client, double timestamp); // VOD only

int rtmp_client_getstatus(void* client);

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
