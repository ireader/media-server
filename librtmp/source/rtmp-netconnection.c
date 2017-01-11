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

	if (out) out = AMFWriteString(out, bytes, command, strlen(command));
	if (out) out = AMFWriteDouble(out, end - out, transactionId);

	if (out) *out++ = AMF_OBJECT;
	if (out && connect->app) out = AMFWriteNamedString(out, end - out, "app", 3, connect->app, strlen(connect->app));
	if (out && connect->flashver) out = AMFWriteNamedString(out, end - out, "flashver", 8, connect->flashver, strlen(connect->flashver));
	if (out && connect->swfUrl) out = AMFWriteNamedString(out, end - out, "swfUrl", 6, connect->swfUrl, strlen(connect->swfUrl));
	if (out && connect->tcUrl) out = AMFWriteNamedString(out, end - out, "tcUrl", 5, connect->tcUrl, strlen(connect->tcUrl));
	if (out && -1 != connect->fpad) out = AMFWriteNamedBoolean(out, end - out, "fpad", 4, connect->fpad);
	if (out && -1 != connect->audioCodecs) out = AMFWriteNamedDouble(out, end - out, "audioCodecs", 11, connect->audioCodecs);
	if (out && -1 != connect->videoCodecs) out = AMFWriteNamedDouble(out, end - out, "videoCodecs", 11, connect->videoCodecs);
	if (out && -1 != connect->videoFunction) out = AMFWriteNamedDouble(out, end - out, "videoFunction", 13, connect->videoFunction);
	if (out && connect->pageUrl) out = AMFWriteNamedString(out, end - out, "pageUrl", 7, connect->pageUrl, strlen(connect->pageUrl));
	if (out) out = AMFWriteNamedDouble(out, end - out, "objectEncoding", 14, connect->objectEncoding);

	if (out && end - out >= 3)
	{
		*out++ = 0;
		*out++ = 0;	/* end of object - 0x00 0x00 0x09 */
		*out++ = AMF_OBJECT_END;
	}

	return out;
}

uint8_t* rtmp_netconnection_connect_reply(uint8_t* out, size_t bytes, int transactionId, const struct rtmp_connect_reply_t* reply)
{
	uint8_t* end = out + bytes;
	const char* command = s_netconnection_command[RTMP_NETCONNECTION_RESULT];

	if (out) out = AMFWriteString(out, bytes, command, strlen(command));
	if (out) out = AMFWriteDouble(out, end - out, transactionId);

	if (out) *out++ = AMF_OBJECT;
	if (out && reply->fmsver) out = AMFWriteNamedString(out, end - out, "fmsver", 6, reply->fmsver, strlen(reply->fmsver));
	if (out && end - out >= 3)
	{
		*out++ = 0;
		*out++ = 0;	/* end of object - 0x00 0x00 0x09 */
		*out++ = AMF_OBJECT_END;
	}

	if (out) *out++ = AMF_OBJECT;
	if (out && reply->code) out = AMFWriteNamedString(out, end - out, "code", 4, reply->code, strlen(reply->code));
	if (out && reply->level) out = AMFWriteNamedString(out, end - out, "level", 5, reply->level, strlen(reply->level));
	if (out && reply->description) out = AMFWriteNamedString(out, end - out, "description", 11, reply->description, strlen(reply->description));
	if (out && end - out >= 3)
	{
		*out++ = 0;
		*out++ = 0;	/* end of object - 0x00 0x00 0x09 */
		*out++ = AMF_OBJECT_END;
	}
	return out;
}

uint8_t* rtmp_netconnection_create_stream(uint8_t* out, size_t bytes, int transactionId)
{
	uint8_t* end = out + bytes;
	const char* command = s_netconnection_command[RTMP_NETCONNECTION_CREATE_STREAM];

	if (out) out = AMFWriteString(out, bytes, command, strlen(command));
	if (out) out = AMFWriteDouble(out, end - out, transactionId);
	if (out) *out++ = AMF_NULL;
	return out;
}

uint8_t* rtmp_netconnection_create_stream_reply(uint8_t* out, size_t bytes, int transactionId, int id)
{
	uint8_t* end = out + bytes;
	const char* command = s_netconnection_command[RTMP_NETCONNECTION_RESULT];

	if (out) out = AMFWriteString(out, bytes, command, strlen(command));
	if (out) out = AMFWriteDouble(out, end - out, transactionId);
	if (out) *out++ = AMF_NULL;
	if (out) out = AMFWriteDouble(out, end - out, id);
	return out;
}
