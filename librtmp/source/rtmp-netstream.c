#include "rtmp-netstream.h"
#include "amf0.h"
#include <stdlib.h>
#include <string.h>

static const char* s_netstream_command[] = 
{
	"onStatus",

	"play", 
	"deleteStream", 
	"closeStream",
	"receiveAudio", 
	"receiveVideo", 
	"publish", 
	"seek",
	"pause",

	"FCPublish",
	"FCUnpublish",
	"FCSubscribe",
	"FCUnsubscribe",
};

enum 
{
	RTMP_NETSTREAM_ONSTATUS = 0,
	RTMP_NETSTREAM_PLAY,
	RTMP_NETSTREAM_DELETE_STREAM,
	RTMP_NETSTREAM_CLOSE_STREAM,
	RTMP_NETSTREAM_RECEIVE_AUDIO,
	RTMP_NETSTREAM_RECEIVE_VIDEO,
	RTMP_NETSTREAM_PUBLISH,
	RTMP_NETSTREAM_SEEK,
	RTMP_NETSTREAM_PAUSE,
	RTMP_NETSTREAM_FCPUBLISH,
	RTMP_NETSTREAM_FCUNPUBLISH,
	RTMP_NETSTREAM_FCSUBSCRIBE,
	RTMP_NETSTREAM_FCUNSUBSCRIBE,
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

	out = AMFWriteString(out, end, command, strlen(command)); // Command Name
	out = AMFWriteDouble(out, end, transactionId); // Transaction ID
	out = AMFWriteNull(out, end); // command object
	out = AMFWriteString(out, end, name, strlen(name)); // Stream Name
	out = AMFWriteDouble(out, end, start); // Start Number
	out = AMFWriteDouble(out, end, duration); // Duration Number
	out = AMFWriteBoolean(out, end, (uint8_t)reset); // Reset Boolean
	return out;
}

// reponse: none
uint8_t* rtmp_netstream_delete_stream(uint8_t* out, size_t bytes, int transactionId, int id)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_DELETE_STREAM];

	out = AMFWriteString(out, end, command, strlen(command)); // Command Name
	out = AMFWriteDouble(out, end, transactionId); // Transaction ID
	out = AMFWriteNull(out, end); // command object
	out = AMFWriteDouble(out, end, id); // Stream ID
	return out;
}

uint8_t* rtmp_netconnection_close_stream(uint8_t* out, size_t bytes, int transactionId, int streamId)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_CLOSE_STREAM];

	out = AMFWriteString(out, end, command, strlen(command));
	out = AMFWriteDouble(out, end, transactionId);
	out = AMFWriteNull(out, end);
	out = AMFWriteDouble(out, end, streamId);
	return out;
}

// reponse: enable-false: none, enable-true: onStatus with NetStream.Seek.Notify/NetStream.Play.Start
uint8_t* rtmp_netstream_receive_audio(uint8_t* out, size_t bytes, int transactionId, int enable)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_RECEIVE_AUDIO];

	out = AMFWriteString(out, end, command, strlen(command)); // Command Name
	out = AMFWriteDouble(out, end, transactionId); // Transaction ID
	out = AMFWriteNull(out, end); // command object
	out = AMFWriteBoolean(out, end, (uint8_t)enable); // Bool Flag
	return out;
}

// reponse: enable-false: none, enable-true: onStatus with NetStream.Seek.Notify/NetStream.Play.Start
uint8_t* rtmp_netstream_receive_video(uint8_t* out, size_t bytes, int transactionId, int enable)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_RECEIVE_VIDEO];

	out = AMFWriteString(out, end, command, strlen(command)); // Command Name
	out = AMFWriteDouble(out, end, transactionId); // Transaction ID
	out = AMFWriteNull(out, end); // command object
	out = AMFWriteBoolean(out, end, (uint8_t)enable); // Bool Flag
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

	out = AMFWriteString(out, end, command, strlen(command)); // Command Name
	out = AMFWriteDouble(out, end, transactionId); // Transaction ID
	out = AMFWriteNull(out, end); // command object
	out = AMFWriteString(out, end, name, strlen(name)); // Publishing Name
	out = AMFWriteString(out, end, streamType, strlen(streamType)); // Publishing Type
	return out;
}

// response: success: onStatus-NetStream.Seek.Notify, failure: _error message
uint8_t* rtmp_netstream_seek(uint8_t* out, size_t bytes, int transactionId, int ms)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_SEEK];
	
	out = AMFWriteString(out, end, command, strlen(command)); // Command Name
	out = AMFWriteDouble(out, end, transactionId); // Transaction ID
	out = AMFWriteNull(out, end); // command object
	out = AMFWriteDouble(out, end, ms); // milliSeconds Number
	return out;
}

// @param[in] pause 1-pausing, 0-resuing
// response: success: onStatus-NetStream.Pause.Notify/NetStream.Unpause.Notify, failure: _error message
uint8_t* rtmp_netstream_pause(uint8_t* out, size_t bytes, int transactionId, int pause, int ms)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_PAUSE];

	out = AMFWriteString(out, end, command, strlen(command)); // Command Name
	out = AMFWriteDouble(out, end, transactionId); // Transaction ID
	out = AMFWriteNull(out, end); // command object
	out = AMFWriteBoolean(out, end, (uint8_t)pause); // pause/unpause
	out = AMFWriteDouble(out, end, ms); // milliSeconds Number
	return out;
}

uint8_t* rtmp_netstream_onstatus(uint8_t* out, size_t bytes, int transactionId, enum rtmp_level_t level, const char* code, const char* description)
{
	uint8_t* end = out + bytes;
	const char* command = s_netstream_command[RTMP_NETSTREAM_ONSTATUS];
	const char* slevel = s_rtmp_level[level % (sizeof(s_rtmp_level) / sizeof(s_rtmp_level[0]))];

	if (NULL == code || NULL == description)
		return NULL;

	out = AMFWriteString(out, end, command, strlen(command)); // Command Name
	out = AMFWriteDouble(out, end, transactionId); // Transaction ID
	out = AMFWriteNull(out, end); // command object

	out = AMFWriteObject(out, end);
	out = AMFWriteNamedString(out, end, "level", 5, slevel, strlen(slevel));
	out = AMFWriteNamedString(out, end, "code", 4, code, strlen(code));
	out = AMFWriteNamedString(out, end, "description", 11, description, strlen(description));
	out = AMFWriteObjectEnd(out, end);
	return out;
}
