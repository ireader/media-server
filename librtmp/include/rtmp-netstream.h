#ifndef _rtmp_netstream_h_
#define _rtmp_netstream_h_

#include <stdint.h>
#include <stddef.h>

uint8_t* rtmp_netstream_play(uint8_t* out, size_t bytes, int transactionId, const char* name, double start, double duration, int reset);
uint8_t* rtmp_netstream_pause(uint8_t* out, size_t bytes, int transactionId, int pause, double ms);
uint8_t* rtmp_netstream_seek(uint8_t* out, size_t bytes, int transactionId, double ms);

uint8_t* rtmp_netstream_receive_audio(uint8_t* out, size_t bytes, int transactionId, int enable);
uint8_t* rtmp_netstream_receive_video(uint8_t* out, size_t bytes, int transactionId, int enable);

enum rtmp_stream_type_t
{
	RTMP_STREAM_LIVE = 0,
	RTMP_STREAM_RECORD,
	RTMP_STREAM_APPEND,
};
uint8_t* rtmp_netstream_publish(uint8_t* out, size_t bytes, int transactionId, const char* playpath, enum rtmp_stream_type_t stream);
uint8_t* rtmp_netstream_delete_stream(uint8_t* out, size_t bytes, int transactionId, int streamId);
uint8_t* rtmp_netconnection_close_stream(uint8_t* out, size_t bytes, int transactionId, int streamId);

uint8_t* rtmp_netstream_release_stream(uint8_t* out, size_t bytes, int transactionId, const char* playpath);
uint8_t* rtmp_netstream_fcpublish(uint8_t* out, size_t bytes, int transactionId, const char* playpath);
uint8_t* rtmp_netstream_fcunpublish(uint8_t* out, size_t bytes, int transactionId, const char* playpath);
uint8_t* rtmp_netstream_fcsubscribe(uint8_t* out, size_t bytes, int transactionId, const char* subscribepath);
uint8_t* rtmp_netstream_fcunsubscribe(uint8_t* out, size_t bytes, int transactionId, const char* subscribepath);

enum rtmp_level_t
{
	RTMP_LEVEL_WARNING = 0,
	RTMP_LEVEL_STATUS,
	RTMP_LEVEL_ERROR,
};
uint8_t* rtmp_netstream_onstatus(uint8_t* out, size_t bytes, int transactionId, enum rtmp_level_t level, const char* code, const char* description);

#endif /* !_rtmp_netstream_h_ */
