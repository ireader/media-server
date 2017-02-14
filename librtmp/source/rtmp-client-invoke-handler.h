#include "rtmp-internal.h"
#include "amf0.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static const char* s_rtmp_command_code[] = {
	"NetConnection.Connect.InvalidApp",
	"NetConnection.Connect.Rejected",

	"NetStream.Failed",
	"NetStream.Play.Failed",
	"NetStream.Play.StreamNotFound",
	"NetStream.Play.Start",
	"NetStream.Play.Stop",
	"NetStream.Play.Complete",
	"NetStream.Play.PublishNotify",
	"NetStream.Play.UnpublishNotify",
	"NetStream.Seek.Notify",
	"NetStream.Pause.Notify",
	"NetStream.Publish.Start",
};

#define AMF_OBJECT_ITEM_VALUE(v, amf_type, amf_name, amf_value, amf_size) { v.type=amf_type; v.name=amf_name; v.value=amf_value; v.size=amf_size; }

// s -> c
static int rtmp_command_onconnect_reply(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	char fmsver[64] = { 0 };
	double capabilities = 0;
	struct amf_object_item_t prop[2];
	AMF_OBJECT_ITEM_VALUE(prop[0], AMF_STRING, "fmsVer", fmsver, sizeof(fmsver));
	AMF_OBJECT_ITEM_VALUE(prop[1], AMF_NUMBER, "capabilities", &capabilities, sizeof(capabilities));

	struct amf_object_item_t info[3];
	AMF_OBJECT_ITEM_VALUE(info[0], AMF_STRING, "code", rtmp->result.code, sizeof(rtmp->result.code));
	AMF_OBJECT_ITEM_VALUE(info[1], AMF_STRING, "level", rtmp->result.level, sizeof(rtmp->result.level));
	AMF_OBJECT_ITEM_VALUE(info[2], AMF_STRING, "description", rtmp->result.description, sizeof(rtmp->result.description));

	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "Properties", prop, sizeof(prop) / sizeof(prop[0]));
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_OBJECT, "Information", info, sizeof(info) / sizeof(info[0]));

	//rtmp->onstatus();
	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// s -> c
static int rtmp_command_oncreate_stream_reply(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes, double *stream_id)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_NUMBER, "streamId", stream_id, 8);

	//rtmp->onstatus();
	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

static int rtmp_command_transaction_find(struct rtmp_t* rtmp, uint32_t transaction)
{
	int i;
	for (i = 0; i < N_TRANSACTIONS; i++)
	{
		if (transaction == rtmp->transactions[i].id)
			return i;
	}

	return -1;
}

// s -> c
static int rtmp_command_onresult(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	int i;
	//struct amf_object_item_t info[3];
	//AMF_OBJECT_ITEM_VALUE(info[0], AMF_STRING, "code", rtmp->result.code, sizeof(rtmp->result.code));
	//AMF_OBJECT_ITEM_VALUE(info[1], AMF_STRING, "level", rtmp->result.level, sizeof(rtmp->result.level));
	//AMF_OBJECT_ITEM_VALUE(info[2], AMF_STRING, "description", rtmp->result.description, sizeof(rtmp->result.description));

	//struct amf_object_item_t items[2];
	//AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	//AMF_OBJECT_ITEM_VALUE(items[1], AMF_OBJECT, "Information", info, sizeof(info) / sizeof(info[0]));

	//if (NULL == amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])))
	//{
	//	printf("%s: read AMF failed\n", __FUNCTION__);
	//	return -1;
	//}

	i = rtmp_command_transaction_find(rtmp, transaction);
	if (-1 == i)
	{
		printf("%s: can't find transaction: %u\n", __FUNCTION__, (unsigned int)transaction);
		return -1;
	}

	switch (rtmp->transactions[i].command)
	{
	case RTMP_NETCONNECTION_CONNECT:
		rtmp_command_onconnect_reply(rtmp, data, bytes);
		// next:
		// 1. releaseStream/FCPublish or serverBW/user control message event buffer time
		// 2. createStream
		// 3. FCSubscribe
		rtmp->u.client.onconnect(rtmp->param);
		break;

	case RTMP_NETCONNECTION_CREATE_STREAM:
		rtmp_command_oncreate_stream_reply(rtmp, data, bytes, &rtmp->stream_id);
		// next: 
		// publish 
		// or play/user control message event buffer time
		rtmp->u.client.oncreate_stream(rtmp->param, (uint32_t)rtmp->stream_id);
		break;

	case RTMP_NETSTREAM_PLAY:
	case RTMP_NETSTREAM_PUBLISH:
		//rtmp->is_play = 1;
		assert(0);
		rtmp->u.client.onnotify(rtmp->param, RTMP_NOTIFY_START);
		break;

	default:
		printf("%s: unknown command: %u\n", __FUNCTION__, (unsigned int)rtmp->transactions[i].command);
		break;
	}

	memset(&rtmp->transactions[i], 0, sizeof(struct rtmp_transaction_t));
	return 0;
}

// s -> c
static int rtmp_command_onerror(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	int i;
	struct amf_object_item_t info[3];
	AMF_OBJECT_ITEM_VALUE(info[0], AMF_STRING, "code", rtmp->result.code, sizeof(rtmp->result.code));
	AMF_OBJECT_ITEM_VALUE(info[1], AMF_STRING, "level", rtmp->result.level, sizeof(rtmp->result.level));
	AMF_OBJECT_ITEM_VALUE(info[2], AMF_STRING, "description", rtmp->result.description, sizeof(rtmp->result.description));

	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_OBJECT, "Information", info, sizeof(info) / sizeof(info[0]));

	//rtmp->onerror();
	if (NULL == amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])))
	{
		printf("%s: read AMF failed\n", __FUNCTION__);
		return -1;
	}

	i = rtmp_command_transaction_find(rtmp, transaction);
	if (-1 == i)
	{
		printf("%s: can't find transaction: %u\n", __FUNCTION__, (unsigned int)transaction);
		return -1;
	}

	memset(&rtmp->transactions[i], 0, sizeof(struct rtmp_transaction_t));
	return 0;
}

// s -> c
static int rtmp_command_onstatus(struct rtmp_t* rtmp, double transaction, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t info[3];
	AMF_OBJECT_ITEM_VALUE(info[0], AMF_STRING, "code", rtmp->result.code, sizeof(rtmp->result.code));
	AMF_OBJECT_ITEM_VALUE(info[1], AMF_STRING, "level", rtmp->result.level, sizeof(rtmp->result.level));
	AMF_OBJECT_ITEM_VALUE(info[2], AMF_STRING, "description", rtmp->result.description, sizeof(rtmp->result.description));

	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0); // Command object
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_OBJECT, "information", info, sizeof(info) / sizeof(info[0])); // Information object

	if (NULL == amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])))
	{
		printf("%s: read AMF failed\n", __FUNCTION__);
		return -1;
	}

	//i = rtmp_command_transaction_find(rtmp, transaction);
	//if (-1 == i)
	//{
	//	printf("%s: can't find transaction: %u\n", __FUNCTION__, (unsigned int)transaction);
	//	return -1;
	//}

	//memset(&rtmp->transactions[i], 0, sizeof(struct rtmp_transaction_t));

	assert(0 == strcmp("error", rtmp->result.level)
		|| 0 == strcmp("status", rtmp->result.level)
		|| 0 == strcmp("warning", rtmp->result.level));

	printf("%s: level: %s, code: %s, description: %s\n", __FUNCTION__, rtmp->result.level, rtmp->result.code, rtmp->result.description);

	if (0 == strcmp("error", rtmp->result.level))
	{
		rtmp->onerror(rtmp->param, -1, rtmp->result.code);
	}
	else
	{
		if (0 == stricmp(rtmp->result.code, "NetStream.Play.Start") || 0 == stricmp(rtmp->result.code, "NetStream.Publish.Start"))
		{
			rtmp->u.client.onnotify(rtmp->param, RTMP_NOTIFY_START);
		}
		else if (0 == stricmp(rtmp->result.code, "NetStream.Seek.Notify"))
		{
			rtmp->u.client.onnotify(rtmp->param, RTMP_NOTIFY_SEEK);
		}
		else if (0 == stricmp(rtmp->result.code, "NetStream.Pause.Notify"))
		{
			rtmp->u.client.onnotify(rtmp->param, RTMP_NOTIFY_PAUSE);
		}
		else if (0 == stricmp(rtmp->result.code, "NetStream.Play.Stop") || 0 == stricmp(rtmp->result.code, "NetStream.Play.Complete"))
		{
			rtmp->u.client.onnotify(rtmp->param, RTMP_NOTIFY_STOP);
		}
		else if (0 == stricmp(rtmp->result.code, "NetStream.Play.PublishNotify") || 0 == stricmp(rtmp->result.code, "NetStream.Play.UnpublishNotify"))
		{
		}
		else if (0 == stricmp(rtmp->result.code, "NetConnection.Connect.InvalidApp")
			|| 0 == stricmp(rtmp->result.code, "NetConnection.Connect.Rejected")
			|| 0 == stricmp(rtmp->result.code, "NetStream.Failed")
			|| 0 == stricmp(rtmp->result.code, "NetStream.Play.Failed")
			|| 0 == stricmp(rtmp->result.code, "NetStream.Play.StreamNotFound"))
		{
			rtmp->onerror(rtmp->param, -1, rtmp->result.code);
		}
	}
	return 0;
}

static int rtmp_command_onbwdone(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[1];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

static int rtmp_command_checkbw(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[1];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

int rtmp_command_transaction_save(struct rtmp_t* rtmp, uint32_t transaction, enum rtmp_command_t command)
{
	int i;
	for (i = 0; i < N_TRANSACTIONS; i++)
	{
		if (0 == rtmp->transactions[i].id)
		{
			rtmp->transactions[i].id = transaction;
			rtmp->transactions[i].command = command;
			return i;
		}
	}

	return -1;
}