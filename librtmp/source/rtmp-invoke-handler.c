#include "rtmp-netconnection.h"
#include "rtmp-internal.h"
#include "amf0.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

struct amf_object_item_t
{
	enum AMFDataType type;
	const char* name;
	void* value;
	size_t size;
};

#define AMF_OBJECT_ITEM_VALUE(v, amf_type, amf_name, amf_value, amf_size) { v.type=amf_type; v.name=amf_name; v.value=amf_value; v.size=amf_size; }

static const uint8_t* amf_read_object(const uint8_t* data, const uint8_t* end, struct amf_object_item_t* items, size_t n);

static const uint8_t* amf_read_item(const uint8_t* data, const uint8_t* end, enum AMFDataType type, struct amf_object_item_t* item)
{
	switch (type)
	{
	case AMF_BOOLEAN:
		return AMFReadBoolean(data, end, (uint8_t*)item->value);

	case AMF_NUMBER:
		return AMFReadDouble(data, end, (double*)item->value);

	case AMF_STRING:
		return AMFReadString(data, end, 0, (char*)item->value, item->size);

	case AMF_LONG_STRING:
		return AMFReadString(data, end, 1, (char*)item->value, item->size);

	case AMF_OBJECT:
		return amf_read_object(data, end, (struct amf_object_item_t*)item->value, item->size);

	case AMF_NULL:
		return data;

	default:
		assert(0);
		return NULL;
	}
}

static const uint8_t* amf_read_object(const uint8_t* data, const uint8_t* end, struct amf_object_item_t* items, size_t n)
{
	uint8_t type;
	uint32_t len;
	size_t i;

	while (data + 2 <= end)
	{
		len = *data++ << 8;
		len |= *data++;
		if (0 == len)
			break; // last item

		if (data + len + 1 > end)
			return NULL; // invalid

		for (i = 0; i < n; i++)
		{
			if (0 == memcmp(items[i].name, data, len) && strlen(items[i].name) == len && data[len] == items[i].type)
				break;
		}

		data += len; // skip name string
		type = *data++;
		if (i < n)
		{
			data = amf_read_item(data, end, type, &items[i]);
		}
		else
		{
			// skip unknown item
			switch (type)
			{
			case AMF_BOOLEAN: data += 1; break;
			case AMF_NUMBER: data += 8; break;
			case AMF_STRING: data += 2 + (data[0] << 8) + data[1]; break;
			case AMF_LONG_STRING: data += 4 + (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3]; break;
			default: return NULL;
			}
		}
	}

	if (data && data < end && AMF_OBJECT_END == *data)
		return data + 1;
	return NULL; // invalid object
}

static const uint8_t* amf_read_items(const uint8_t* data, const uint8_t* end, struct amf_object_item_t* items, size_t count)
{
	size_t i;
	uint8_t type;
	for(i = 0; i < count && data && data < end; i++)
	{
		type = *data++;
		if (type != items[i].type && !(AMF_OBJECT == items[i].type && AMF_NULL == type))
			return NULL;

		data = amf_read_item(data, end, type, &items[i]);
	}

	return data;
}

// s -> c
static int rtmp_command_onerror(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t info[3];
	AMF_OBJECT_ITEM_VALUE(info[0], AMF_STRING, "code", rtmp->result.code, sizeof(rtmp->result.code));
	AMF_OBJECT_ITEM_VALUE(info[1], AMF_STRING, "level", rtmp->result.level, sizeof(rtmp->result.level));
	AMF_OBJECT_ITEM_VALUE(info[2], AMF_STRING, "description", rtmp->result.description, sizeof(rtmp->result.description));

	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_OBJECT, "Information", info, sizeof(info) / sizeof(info[0]));

	//rtmp->onerror();
	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// c -> s
static int rtmp_command_onconnect(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
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

// s -> c
static int rtmp_command_onconnect_reply(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	char fmsver[64] = { 0 };
	struct amf_object_item_t prop[1];
	AMF_OBJECT_ITEM_VALUE(prop[0], AMF_STRING, "fmsver", fmsver, sizeof(fmsver));

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

// c -> s
static int rtmp_command_oncreate_stream(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[1];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);

	//rtmp->oncreate_stream();
	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// s -> c
static int rtmp_command_oncreate_stream_reply(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_NUMBER, "streamId", &rtmp->stream_id, 8);

	//rtmp->onstatus();
	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// s -> c
static int rtmp_command_onstatus(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t info[3];
	AMF_OBJECT_ITEM_VALUE(info[0], AMF_STRING, "code", rtmp->result.code, sizeof(rtmp->result.code));
	AMF_OBJECT_ITEM_VALUE(info[1], AMF_STRING, "level", rtmp->result.level, sizeof(rtmp->result.level));
	AMF_OBJECT_ITEM_VALUE(info[2], AMF_STRING, "description", rtmp->result.description, sizeof(rtmp->result.description));

	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0); // Command object
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_OBJECT, "information", info, sizeof(info) / sizeof(info[0])); // Information object

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// c -> s
static int rtmp_command_onplay(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
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

// c -> s
static int rtmp_command_ondelete_stream(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_NUMBER, "streamId", &rtmp->stream_id, 8);

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// c -> s
static int rtmp_command_onreceive_audio(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_BOOLEAN, "boolean", &rtmp->receiveAudio, 1);

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// c -> s
static int rtmp_command_onreceive_video(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_BOOLEAN, "boolean", &rtmp->receiveVideo, 1);

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// c -> s
static int rtmp_command_onpublish(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[3];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_STRING, "name", rtmp->stream_name, sizeof(rtmp->stream_name));
	AMF_OBJECT_ITEM_VALUE(items[2], AMF_STRING, "type", rtmp->stream_type, sizeof(rtmp->stream_type));

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// c -> s
static int rtmp_command_onseek(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_NUMBER, "milliSeconds", &rtmp->milliSeconds, 8);

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// c -> s
static int rtmp_command_onpause(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
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

// c -> s
static int rtmp_command_onfcpublish(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_STRING, "playpath", &rtmp->stream_name, sizeof(rtmp->stream_name));

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// c -> s
static int rtmp_command_onfcunpublish(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_STRING, "playpath", &rtmp->stream_name, sizeof(rtmp->stream_name));

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// c -> s
static int rtmp_command_onfcsubscribe(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_STRING, "subscribepath", &rtmp->stream_name, sizeof(rtmp->stream_name));

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
}

// c -> s
static int rtmp_command_onfcunsubscribe(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes)
{
	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0);
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_STRING, "subscribepath", &rtmp->stream_name, sizeof(rtmp->stream_name));

	return amf_read_items(data, data + bytes, items, sizeof(items) / sizeof(items[0])) ? 0 : -1;
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

struct rtmp_command_handler_t
{
	const char* command;
	int (*handler)(struct rtmp_t* rtmp, const uint8_t* data, uint32_t bytes);
};

const static struct rtmp_command_handler_t s_command_handler[] = {
	//{ "_result",		rtmp_command_onresult },
	{ "_error",			rtmp_command_onerror },

	{ "connect",		rtmp_command_onconnect },
	{ "createStream",	rtmp_command_oncreate_stream },

	{ "onStatus",		rtmp_command_onstatus },
	{ "play",			rtmp_command_onplay },
	{ "deleteStream",	rtmp_command_ondelete_stream },
	{ "receiveAudio",	rtmp_command_onreceive_audio },
	{ "receiveVideo",	rtmp_command_onreceive_video },
	{ "publish",		rtmp_command_onpublish },
	{ "seek",			rtmp_command_onseek },
	{ "pause",			rtmp_command_onpause },

	{ "FCPublish",		rtmp_command_onfcpublish },
	{ "FCUnpublish",	rtmp_command_onfcunpublish },
	{ "FCSubscribe",	rtmp_command_onfcsubscribe },
	{ "FCUnsubscribe",	rtmp_command_onfcunsubscribe },
	{ "onBWDone",		rtmp_command_onbwdone },
	{ "_checkbw",		rtmp_command_checkbw },
};

int rtmp_invoke_handler(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* data)
{
	int i;
	char command[64] = { 0 };
	double transaction_id = (double)(uint32_t)(-1);
	const uint8_t *end = data + header->length;

	struct amf_object_item_t items[2];
	AMF_OBJECT_ITEM_VALUE(items[0], AMF_STRING, "command", command, sizeof(command));
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_NUMBER, "transactionId", &transaction_id, 8);

	data = amf_read_items(data, end, items, sizeof(items) / sizeof(items[0]));
	if (!data || (uint32_t)(-1) == (uint32_t)transaction_id)
		return -1; // invalid data

	for (i = 0; i < sizeof(s_command_handler) / sizeof(s_command_handler[0]); i++)
	{
		if (0 == strcmp(command, s_command_handler[i].command))
		{
			return s_command_handler[i].handler(rtmp, data, end - data);
		}
	}

	assert(0);
	return ENOENT; // not found
}
