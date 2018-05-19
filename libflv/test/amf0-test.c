#include "amf0.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define AMF_OBJECT_ITEM_VALUE(v, amf_type, amf_name, amf_value, amf_size) { v.type=amf_type; v.name=amf_name; v.value=amf_value; v.size=amf_size; }

struct rtmp_result_t
{
	char code[64]; // NetStream.Play.Start
	char level[8]; // warning/status/error
	char description[256];
};

static int amf0_get(const char* file, void* amf0, size_t bytes)
{
	int r;
	FILE* fp;
	fp = fopen(file, "rb");
	if (NULL == fp)
		return 0;

	r = fread(amf0, 1, bytes, fp);
	fclose(fp);
	return r;
}

void amf0_test2(void)
{
	int r;
	uint8_t* end;
	static uint8_t amf0[2 * 1024];
	struct rtmp_result_t result;
	struct amf_object_item_t info[3];
	struct amf_object_item_t items[2];

	AMF_OBJECT_ITEM_VALUE(info[0], AMF_STRING, "code", result.code, sizeof(result.code));
	AMF_OBJECT_ITEM_VALUE(info[1], AMF_STRING, "level", result.level, sizeof(result.level));
	AMF_OBJECT_ITEM_VALUE(info[2], AMF_STRING, "description", result.description, sizeof(result.description));

	AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "command", NULL, 0); // Command object
	AMF_OBJECT_ITEM_VALUE(items[1], AMF_OBJECT, "information", info, sizeof(info) / sizeof(info[0])); // Information object

	r = amf0_get("../libflv/test/rtmp.onStatus.amf0", amf0, sizeof(amf0));

	end = amf0 + r;
	assert(end == amf_read_items(amf0, end, items, sizeof(items) / sizeof(items[0])));
	assert(0 == strcmp(result.code, "NetStream.Play.Reset"));
	assert(0 == strcmp(result.level, "status"));
	assert(0 == strcmp(result.description, "Playing and resetting 92f509c10c112171f935?token=3129bc162ee05a1353f7&secret=15b1bca0997ab790656c903493cada3b&ckey=17e23e4fd0bb5b54a2434fd1514343ee"));
}
