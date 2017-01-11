#include "rtmp-netstream.h"
#include "amf0.h"
#include <stdlib.h>
#include <string.h>

static const char* s_netstream_command[] = 
{
	"onStatus",
	"play", 
	"deleteStream", 
	"receiveAudio", 
	"receiveVideo", 
	"publish", 
	"seek",
	"pause",
};

enum 
{
	RTMP_NETSTREAM_ONSTATUS = 0,
	RTMP_NETSTREAM_PLAY,
	RTMP_NETSTREAM_DELETE_STREAM,
	RTMP_NETSTREAM_RECEIVE_AUDIO,
	RTMP_NETSTREAM_RECEIVE_VIDEO,
	RTMP_NETSTREAM_PUBLISH,
	RTMP_NETSTREAM_SEEK,
	RTMP_NETSTREAM_PAUSE,
};

static const char* s_rtmp_level[] = { "warning", "status", "error" };
static const char* s_rtmp_stream[] = { "live", "record", "append" };

// @param[in] streamName flv:sample, mp3:sample, H.264/AAC: mp4:sample.m4v
// @param[in] start -2-live/vod, -1-live only, >=0-seek position
// @param[in] duration <=-1-all, 0-single frame, >0-period
// @param[in] reset 1-flush any previous playlist, 0-don't flush
uint8_t* rtmp_netstream_play(uint8_t* out, size_t bytes, int transactionId, const char* name, int start, int duration, int reset)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_PLAY];
	
	if (NULL == name)
		return NULL;

	out = AMFWriteString(out, bytes, command, strlen(command)); // Command Name
	if (out) out = AMFWriteDouble(out, end - out, transactionId); // Transaction ID
	if (out) *out++ = AMF_NULL; // command object
	if (out) out = AMFWriteString(out, bytes, name, strlen(name)); // Stream Name
	if (out) out = AMFWriteDouble(out, end - out, start); // Start Number
	if (out) out = AMFWriteDouble(out, end - out, duration); // Duration Number
	if (out) out = AMFWriteBoolean(out, end - out, reset); // Reset Boolean

	return out;
}

// reponse: none
uint8_t* rtmp_netstream_delete_stream(uint8_t* out, size_t bytes, int transactionId, int id)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_DELETE_STREAM];

	if (out) out = AMFWriteString(out, bytes, command, strlen(command)); // Command Name
	if (out) out = AMFWriteDouble(out, end - out, transactionId); // Transaction ID
	if (out) *out++ = AMF_NULL; // command object
	if (out) out = AMFWriteDouble(out, end - out, id); // Stream ID
	return out;
}

// reponse: enable-false: none, enable-true: onStatus with NetStream.Seek.Notify/NetStream.Play.Start
uint8_t* rtmp_netstream_receive_audio(uint8_t* out, size_t bytes, int transactionId, int enable)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_RECEIVE_AUDIO];

	if (out) out = AMFWriteString(out, bytes, command, strlen(command)); // Command Name
	if (out) out = AMFWriteDouble(out, end - out, transactionId); // Transaction ID
	if (out) *out++ = AMF_NULL; // command object
	if (out) out = AMFWriteBoolean(out, end - out, enable); // Bool Flag
	return out;
}

// reponse: enable-false: none, enable-true: onStatus with NetStream.Seek.Notify/NetStream.Play.Start
uint8_t* rtmp_netstream_receive_video(uint8_t* out, size_t bytes, int transactionId, int enable)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_RECEIVE_VIDEO];

	if (out) out = AMFWriteString(out, bytes, command, strlen(command)); // Command Name
	if (out) out = AMFWriteDouble(out, end - out, transactionId); // Transaction ID
	if (out) *out++ = AMF_NULL; // command object
	if (out) out = AMFWriteBoolean(out, end - out, enable); // Bool Flag
	return out;
}

// response: onStatus beginning of publish
uint8_t* rtmp_netstream_publish(uint8_t* out, size_t bytes, int transactionId, const char* name, enum rtmp_stream_type_t stream)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_PUBLISH];
	const char* streamType = s_rtmp_stream[stream % (sizeof(s_rtmp_stream) / sizeof(s_rtmp_stream[0]))];

	if (NULL == name)
		return NULL;

	if (out) out = AMFWriteString(out, bytes, command, strlen(command)); // Command Name
	if (out) out = AMFWriteDouble(out, end - out, transactionId); // Transaction ID
	if (out) *out++ = AMF_NULL; // command object
	if (out) out = AMFWriteString(out, end - out, name, strlen(name)); // Publishing Name
	if (out) out = AMFWriteString(out, end - out, streamType, strlen(streamType)); // Publishing Type
	return out;
}

// response: success: onStatus-NetStream.Seek.Notify, failure: _error message
uint8_t* rtmp_netstream_seek(uint8_t* out, size_t bytes, int transactionId, int ms)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_SEEK];
	
	if (out) out = AMFWriteString(out, bytes, command, strlen(command)); // Command Name
	if (out) out = AMFWriteDouble(out, end - out, transactionId); // Transaction ID
	if (out) *out++ = AMF_NULL; // command object
	if (out) out = AMFWriteDouble(out, end - out, ms); // milliSeconds Number
	return out;
}

// @param[in] pause 1-pausing, 0-resuing
// response: success: onStatus-NetStream.Pause.Notify/NetStream.Unpause.Notify, failure: _error message
uint8_t* rtmp_netstream_pause(uint8_t* out, size_t bytes, int transactionId, int pause, int ms)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_PAUSE];

	if (out) out = AMFWriteString(out, bytes, command, strlen(command)); // Command Name
	if (out) out = AMFWriteDouble(out, end - out, transactionId); // Transaction ID
	if (out) *out++ = AMF_NULL; // command object
	if (out) out = AMFWriteBoolean(out, end - out, pause); // milliSeconds Number
	if (out) out = AMFWriteDouble(out, end - out, ms); // milliSeconds Number
	return out;
}

uint8_t* rtmp_netstream_onstatus(uint8_t* out, size_t bytes, int transactionId, enum rtmp_level_t level, const char* code, const char* description)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_ONSTATUS];
	const char* slevel = s_rtmp_level[level % (sizeof(s_rtmp_level) / sizeof(s_rtmp_level[0]))];

	if (NULL == code || NULL == description)
		return NULL;

	if (out) out = AMFWriteString(out, bytes, command, strlen(command)); // Command Name
	if (out) out = AMFWriteDouble(out, end - out, transactionId); // Transaction ID
	if (out) *out++ = AMF_NULL; // command object

	if (out) *out++ = AMF_OBJECT;
	if (out) out = AMFWriteNamedString(out, end - out, "level", 5, slevel, strlen(slevel));
	if (out) out = AMFWriteNamedString(out, end - out, "code", 4, code, strlen(code));
	if (out) out = AMFWriteNamedString(out, end - out, "description", 11, description, strlen(description));
	if (out && end - out >= 3)
	{
		*out++ = 0;
		*out++ = 0;	/* end of object - 0x00 0x00 0x09 */
		*out++ = AMF_OBJECT_END;
	}

	return out;
}
