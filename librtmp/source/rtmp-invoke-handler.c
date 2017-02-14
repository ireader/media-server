#include "rtmp-netconnection.h"
#include "rtmp-internal.h"
#include "amf0.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "rtmp-client-invoke-handler.h"

// connect request parser
static int rtmp_command_onconnect(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	uint8_t fpad = 0;
	double audioCodecs = 0.0, videoCodecs = 0.0, videoFunction = 0.0;
	char app[64] = { 0 };
	char flashver[16] = { 0 };
	char tcUrl[256] = { 0 };
	struct amf_object_item_t commands[7];
	AMF_OBJECT_ITEM_VALUE(commands[0], AMF_STRING, "app", app, sizeof(app));
	AMF_OBJECT_ITEM_VALUE(commands[1], AMF_STRING, "flashver", flashver, sizeof(flashver));
	AMF_OBJECT_ITEM_VALUE(commands[2], AMF_STRING, "tcUrl", tcUrl, sizeof(tcUrl));
	AMF_OBJECT_ITEM_VALUE(commands[3], AMF_BOOLEAN, "fpad", &fpad, 1);
	AMF_OBJECT_ITEM_VALUE(commands[4], AMF_NUMBER, "audioCodecs", &audioCodecs, 8);
	AMF_OBJECT_ITEM_VALUE(commands[5], AMF_NUMBER, "videoCodecs", &videoCodecs, 8);
	AMF_OBJECT_ITEM_VALUE(commands[6], AMF_NUMBER, "videoFunction", &videoFunction, 8);

	struct amf_object_item_t items[1];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", commands, sizeof(commands) / sizeof(commands[0]));

	//rtmp->onconnect();
	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// createStream request parser
static int rtmp_command_oncreate_stream(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[1];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);

	//rtmp->oncreate_stream();
	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// play request parser
static int rtmp_command_onplay(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	uint8_t reset = 0;
	double start, duration;
	struct amf_object_item_t items[5];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_STRING, "stream", rtmp->result.code, sizeof(rtmp->result.code));
	AMF_OBJECT_ITEM_VALUE(items[2], AMF_NUMBER, "start", &start, 8);
	AMF_OBJECT_ITEM_VALUE(items[3], AMF_NUMBER, "duration", &duration, 8);
	AMF_OBJECT_ITEM_VALUE(items[4], AMF_BOOLEAN, "reset", &reset, 1);

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// deleteStream request parser
static int rtmp_command_ondelete_stream(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_NUMBER, "streamId", &rtmp->stream_id, 8);

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// receiveAudio request parser
static int rtmp_command_onreceive_audio(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_BOOLEAN, "boolean", &rtmp->receiveAudio, 1);

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// receiveVideo request parser
static int rtmp_command_onreceive_video(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_BOOLEAN, "boolean", &rtmp->receiveVideo, 1);

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// publish request parser
static int rtmp_command_onpublish(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[3];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_STRING, "name", rtmp->playpath, sizeof(rtmp->playpath));
	AMF_OBJECT_ITEM_VALUE(items[2], AMF_STRING, "type", rtmp->stream_type, sizeof(rtmp->stream_type));

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// seek request parser
static int rtmp_command_onseek(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_NUMBER, "milliSeconds", &rtmp->milliSeconds, 8);

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// pause request parser
static int rtmp_command_onpause(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[3];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_BOOLEAN, "pause", &rtmp->pause, 1);
	AMF_OBJECT_ITEM_VALUE(items[2], AMF_NUMBER, "milliSeconds", &rtmp->milliSeconds, 8);

	if (NULL != amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])))
	{
		//return rtmp->onpacket(rtmp->param, header, data);
	}
	return -1;
}

// FCPublish request parser
static int rtmp_command_onfcpublish(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_STRING, "playpath", &rtmp->playpath, sizeof(rtmp->playpath));

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// FCUnpublish request parser
static int rtmp_command_onfcunpublish(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_STRING, "playpath", &rtmp->playpath, sizeof(rtmp->playpath));

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// FCSubscribe request parser
static int rtmp_command_onfcsubscribe(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_STRING, "subscribepath", &rtmp->playpath, sizeof(rtmp->playpath));

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// FCUnsubscribe request parser
static int rtmp_command_onfcunsubscribe(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_STRING, "subscribepath", &rtmp->playpath, sizeof(rtmp->playpath));

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

struct rtmp_command_handler_t
{
	const char* name;
	enum rtmp_command_t command;
	int (*handler)(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes);
};

const static struct rtmp_command_handler_t s_command_handler[] = {
	// client side
	{ "_result",		0, rtmp_command_onresult },
	{ "_error",			0, rtmp_command_onerror },
	{ "onStatus",		0, rtmp_command_onstatus },

//	{ "close",			0, rtmp_command_onclose },

//	{ "onBWDone",		0, rtmp_command_onsendcheckbw },
//	{ "_onbwcheck",		0, rtmp_command_onbwcheck },
//	{ "_onbwdone",		0, rtmp_command_onbwdone },

//	{ "ping",			0, rtmp_command_onping },

//	{ "playlist_ready",	0, rtmp_command_onplaylist },

	{ "onFCSubscribe",	0, rtmp_command_onfcsubscribe },
	{ "onFCUnsubscribe",0, rtmp_command_onfcunsubscribe },

	// server side
	{ "connect",		RTMP_NETCONNECTION_CONNECT,			rtmp_command_onconnect },
	{ "createStream",	RTMP_NETCONNECTION_CREATE_STREAM,	rtmp_command_oncreate_stream },

	{ "play",			RTMP_NETSTREAM_PLAY,				rtmp_command_onplay },
	{ "deleteStream",	RTMP_NETSTREAM_DELETE_STREAM,		rtmp_command_ondelete_stream },
	{ "receiveAudio",	RTMP_NETSTREAM_RECEIVE_VIDEO,		rtmp_command_onreceive_audio },
	{ "receiveVideo",	RTMP_NETSTREAM_RECEIVE_VIDEO,		rtmp_command_onreceive_video },
	{ "publish",		RTMP_NETSTREAM_PUBLISH,				rtmp_command_onpublish },
	{ "seek",			RTMP_NETSTREAM_SEEK,				rtmp_command_onseek },
	{ "pause",			RTMP_NETSTREAM_PAUSE,				rtmp_command_onpause },

	{ "FCPublish",		RTMP_NETSTREAM_FCPUBLISH,			rtmp_command_onfcpublish },
	{ "FCUnpublish",	RTMP_NETSTREAM_FCUNPUBLISH,			rtmp_command_onfcunpublish },
	{ "FCSubscribe",	RTMP_NETSTREAM_FCSUBSCRIBE,			rtmp_command_onfcsubscribe },
	{ "FCUnsubscribe",	RTMP_NETSTREAM_FCUNSUBSCRIBE,		rtmp_command_onfcunsubscribe },
};

int rtmp_invoke_handler(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* data)
{
	int i, r;
	char command[64] = { 0 };
	double transaction = -1.0;
	const uint8_t *end = data + header->length;

	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_STRING, "command", command, sizeof(command));
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_NUMBER, "transactionId", &transaction, sizeof(double));

	data = amf_read_items(data, end, items, sizeof(items) / sizeof(items[0]));
	if (!data || -1.0 == transaction)
		return -1; // invalid data

	for (i = 0; i < sizeof(s_command_handler) / sizeof(s_command_handler[0]); i++)
	{
		if (0 == strcmp(command, s_command_handler[i].name))
		{
			r = s_command_handler[i].handler(rtmp, transaction, data, end - data);
			if(r >= 0 && 0 != s_command_handler[i].command)
			{
				// server side
				r = rtmp->u.server.onhandler(rtmp->param, s_command_handler[i].command);
			}
			return r;
		}
	}

	assert(0);
	return ENOENT; // not found
}
