#include "rtmp-netconnection.h"
#include "amf0.h"
#include <stdlib.h>
#include <string.h>

static const char* s_netconnection_command[] = { "_result", "connect", "createStream" };

enum {
	RTMP_NETCONNECTION_RESULT = 0,
	RTMP_NETCONNECTION_CONNECT,
	RTMP_NETCONNECTION_CREATE_STREAM,
};

uint8_t* rtmp_netconnection_connect(uint8_t* out, size_t bytes, int transactionId, const struct rtmp_connect_t* connect)
{
	uint8_t* end = out + bytes;
	const char* command = s_netconnection_command[RTMP_NETCONNECTION_CONNECT];

	out = AMFWriteString(out, end, command, strlen(command));
	out = AMFWriteDouble(out, end, transactionId);

	out = AMFWriteObject(out, end);
	out = AMFWriteNamedString(out, end, "app", 3, connect->app, strlen(connect->app));
	out = AMFWriteNamedString(out, end, "flashver", 8, connect->flashver, strlen(connect->flashver));
	out = AMFWriteNamedString(out, end, "swfUrl", 6, connect->swfUrl, strlen(connect->swfUrl));
	out = AMFWriteNamedString(out, end, "tcUrl", 5, connect->tcUrl, strlen(connect->tcUrl));
	out = AMFWriteNamedBoolean(out, end, "fpad", 4, connect->fpad);
	out = AMFWriteNamedDouble(out, end, "audioCodecs", 11, connect->audioCodecs);
	out = AMFWriteNamedDouble(out, end, "videoCodecs", 11, connect->videoCodecs);
	out = AMFWriteNamedDouble(out, end, "videoFunction", 13, connect->videoFunction);
	out = AMFWriteNamedString(out, end, "pageUrl", 7, connect->pageUrl, strlen(connect->pageUrl));
	out = AMFWriteNamedDouble(out, end, "objectEncoding", 14, RTMP_ENCODING_AMF_0);
	out = AMFWriteObjectEnd(out, end);
	return out;
}

uint8_t* rtmp_netconnection_connect_reply(uint8_t* out, size_t bytes, int transactionId, const struct rtmp_connect_reply_t* reply)
{
	uint8_t* end = out + bytes;
	const char* command = s_netconnection_command[RTMP_NETCONNECTION_RESULT];

	out = AMFWriteString(out, end, command, strlen(command));
	out = AMFWriteDouble(out, end, transactionId);

	out = AMFWriteObject(out, end);
	out = AMFWriteNamedString(out, end, "fmsVer", 6, reply->fmsver, strlen(reply->fmsver));
	out = AMFWriteNamedDouble(out, end, "capabilities", 12, reply->capabilities);
	out = AMFWriteObjectEnd(out, end);

	out = AMFWriteObject(out, end);
	out = AMFWriteNamedString(out, end, "code", 4, reply->code, strlen(reply->code));
	out = AMFWriteNamedString(out, end, "level", 5, reply->level, strlen(reply->level));
	out = AMFWriteNamedString(out, end, "description", 11, reply->description, strlen(reply->description));
	out = AMFWriteObjectEnd(out, end);
	return out;
}

uint8_t* rtmp_netconnection_create_stream(uint8_t* out, size_t bytes, int transactionId)
{
	uint8_t* end = out + bytes;
	const char* command = s_netconnection_command[RTMP_NETCONNECTION_CREATE_STREAM];

	out = AMFWriteString(out, end, command, strlen(command));
	out = AMFWriteDouble(out, end, transactionId);
	out = AMFWriteNull(out, end);
	return out;
}

uint8_t* rtmp_netconnection_create_stream_reply(uint8_t* out, size_t bytes, int transactionId, int id)
{
	uint8_t* end = out + bytes;
	const char* command = s_netconnection_command[RTMP_NETCONNECTION_RESULT];

	out = AMFWriteString(out, end, command, strlen(command));
	out = AMFWriteDouble(out, end, transactionId);
	out = AMFWriteNull(out, end);
	out = AMFWriteDouble(out, end, id);
	return out;
}
