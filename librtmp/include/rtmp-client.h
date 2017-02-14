#ifndef _rtmp_client_h_
#define _rtmp_client_h_

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct rtmp_client_handler_t
{
	// network implemention
	int (*send)(void* param, const void* data, size_t bytes);

	void (*onerror)(void* param, int code, const char* msg);

	// VOD only
	void (*onvideo)(void* param, const void* data, size_t bytes, uint32_t timestamp);
	void (*onaudio)(void* param, const char* data, size_t bytes, uint32_t timestamp);
	void (*onmeta)(void* param, const char* data, size_t bytes);
};

/// setup URL and connect server(timeout???)
void* rtmp_client_create(const char* appname, const char* playpath, const char* tcurl, void* param, const struct rtmp_client_handler_t* handler);
void rtmp_client_destroy(void** client);

int rtmp_client_input(void* client, const void* data, size_t bytes);

// @param[in] vod 1-VOD(pull from server), 0-Publish(push stream to server)
int rtmp_client_start(void* client, int vod);
int rtmp_client_stop(void* client);
int rtmp_client_pause(void* client, int pause);
int rtmp_client_seek(void* client, double timestamp);

int rtmp_client_getstatus(void* client);

///@param[in] audio: AAC AudioSpecificConfig
///@param[in] abytes: AudioSpecificConfig length in bytes
///@param[in] video: H264 AVCDecoderConfigurationRecord
///@param[in] vbytes: AVCDecoderConfigurationRecord length in bytes
///@return 0-ok, other-error
int rtmp_client_set_header(void* client, const void* audio, size_t abytes, const void* video, size_t vbytes);

///@param[in] video: H264 packet(include startcode(00 00 00 01))
///@param[in] bytes: video length in bytes
///@return 0-ok, other-error
int rtmp_push_video(void* client, const void* video, size_t bytes, uint32_t pts, uint32_t dts);

// @param[in] audio: AAC audio packet(include AAC packet header(ADTS))
// @param[in] bytes: video length in bytes
///@return 0-ok, other-error
int rtmp_push_audio(void* client, const void* audio, size_t bytes, uint32_t pts, uint32_t dts);

void rtmp_client_getserver(void* client, char ip[65]);

///create AudioSpecificConfig from AAC data
///@return 0-error
size_t rtmp_client_make_AudioSpecificConfig(void* out, const void* audio, size_t bytes);

/// creaate AVCDecoderConfigurationRecord from H.264 sps/pps nalu(all nalu must start with 00 00 00 01)
///@return 0-error
size_t rtmp_client_make_AVCDecoderConfigurationRecord(const void* video, size_t bytes, void* out, size_t osize);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtmp_client_h_ */
