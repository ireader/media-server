#ifndef _rtmp_netstream_h_
#define _rtmp_netstream_h_

#include <stdint.h>
#include <stddef.h>

uint8_t* rtmp_netstream_play(uint8_t* out, size_t bytes, int transactionId, const char* name, int start, int duration, int reset);
uint8_t* rtmp_netstream_pause(uint8_t* out, size_t bytes, int transactionId, int pause, int ms);
uint8_t* rtmp_netstream_seek(uint8_t* out, size_t bytes, int transactionId, int ms);

uint8_t* rtmp_netstream_receive_audio(uint8_t* out, size_t bytes, int transactionId, int enable);
uint8_t* rtmp_netstream_receive_video(uint8_t* out, size_t bytes, int transactionId, int enable);

enum rtmp_stream_type_t
{
	RTMP_STREAM_LIVE = 0,
	RTMP_STREAM_RECORD,
	RTMP_STREAM_APPEND,
};
uint8_t* rtmp_netstream_publish(uint8_t* out, size_t bytes, int transactionId, const char* name, enum rtmp_stream_type_t stream);
uint8_t* rtmp_netstream_delete_stream(uint8_t* out, size_t bytes, int transactionId, int streamId);
uint8_t* rtmp_netconnection_close_stream(uint8_t* out, size_t bytes, int transactionId, int streamId);

enum rtmp_level_t
{
	RTMP_LEVEL_WARNING = 0,
	RTMP_LEVEL_STATUS,
	RTMP_LEVEL_ERROR,
};
uint8_t* rtmp_netstream_onstatus(uint8_t* out, size_t bytes, int transactionId, enum rtmp_level_t level, const char* code, const char* description);

#endif /* !_rtmp_netstream_h_ */
