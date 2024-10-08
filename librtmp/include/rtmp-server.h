#ifndef _rtmp_server_h_
#define _rtmp_server_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTMP_SERVER_ASYNC_START 0x12345678 // magic number, user call rtmp_server_start

#define RTMP_SERVER_START_RECONNECT 1 // use with RTMP_SERVER_ASYNC_START

typedef struct rtmp_server_t rtmp_server_t;

struct rtmp_server_handler_t
{
	///network implementation
	///@param[in] payload rtmp payload, could be NULL
	///@param[in] bytes rtmp payload size in byte, could be 0
	///@return >0-sent bytes, <0-error
	int (*send)(void* param, const void* header, size_t len, const void* payload, size_t bytes);

	///@return 0-ok, other-error
	//int (*oncreate_stream)(void* param, uint32_t* stream_id);
	//int (*ondelete_stream)(void* param, uint32_t stream_id);

	///pull(server -> client)
	///@return 0-ok, RTMP_SERVER_ASYNC_START-async mode(must call rtmp_server_start next), other-error
	int (*onplay)(void* param, const char* app, const char* stream, double start, double duration, uint8_t reset);
	int (*onpause)(void* param, int pause, uint32_t ms);
	int (*onseek)(void* param, uint32_t ms);

	///push(client -> server)
	///@param[in] type: live/record/append
	///@return 0-ok, RTMP_SERVER_ASYNC_START-async mode(must call rtmp_server_start next), other-error
	int (*onpublish)(void* param, const char* app, const char* stream, const char* type);
	///@param[in] data FLV VideoTagHeader + AVCVIDEOPACKET: AVCDecoderConfigurationRecord(ISO 14496-15) / One or more NALUs(four-bytes length + NALU)
	///@return 0-ok, other-error
	int (*onvideo)(void* param, const void* data, size_t bytes, uint32_t timestamp);
	///@param[in] data FLV AudioTagHeader + AACAUDIODATA: AudioSpecificConfig(14496-3) / Raw AAC frame data in UI8
	///@return 0-ok, other-error
	int (*onaudio)(void* param, const void* data, size_t bytes, uint32_t timestamp);
	///@param[in] data FLV onMetaData
	///@return 0-ok, other-error
	int (*onscript)(void* param, const void* data, size_t bytes, uint32_t timestamp);

	///@param[out] duration stream length in seconds
	///@return 0-ok, other-error
	int (*ongetduration)(void* param, const char* app, const char* stream, double* duration);
};

rtmp_server_t* rtmp_server_create(void* param, const struct rtmp_server_handler_t* handler);

void rtmp_server_destroy(rtmp_server_t* rtmp);

int rtmp_server_getstate(rtmp_server_t* rtmp);

/// @param[in] rtmp rtmp_server_create instance
/// @param[in] data rtmp chunk stream data
/// @param[in] bytes data length
/// @return 0-ok, other-error
int rtmp_server_input(rtmp_server_t* rtmp, const uint8_t* data, size_t bytes);

/// send audio/video data(VOD only)
/// @param[in] rtmp rtmp_server_create instance
/// @return 0-ok, other-error
int rtmp_server_send_audio(rtmp_server_t* rtmp, const void* data, size_t bytes, uint32_t timestamp);
int rtmp_server_send_video(rtmp_server_t* rtmp, const void* data, size_t bytes, uint32_t timestamp);
int rtmp_server_send_script(rtmp_server_t* rtmp, const void* data, size_t bytes, uint32_t timestamp);

/// [OPTIONAL] must call on onplay/onpublish return RTMP_SERVER_ASYNC_START
/// @param[in] code 0-ok, RTMP_SERVER_START_RECONNECT-reconnect, other-error
/// @param[in] msg error message, or tcurl if code == RTMP_SERVER_START_RECONNECT
/// @return 0-ok, other-error
int rtmp_server_start(rtmp_server_t* rtmp, int code, const char* msg);

#ifdef __cplusplus
}
#endif
#endif /* !_rtmp_server_h_ */
