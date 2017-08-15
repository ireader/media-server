#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
#include <assert.h>
#include <stdio.h>
#include <list>
#include <string.h>

static void* ts_alloc(void* /*param*/, size_t bytes)
{
	static char s_buffer[188];
	assert(bytes <= sizeof(s_buffer));
	return s_buffer;
}

static void ts_free(void* /*param*/, void* /*packet*/)
{
	return;
}

static void ts_write(void* param, const void* packet, size_t bytes)
{
	fwrite(packet, bytes, 1, (FILE*)param);
}

inline const char* ts_type(int type)
{
	switch (type)
	{
	case PSI_STREAM_MP3: return "MP3";
	case PSI_STREAM_AAC: return "AAC";
	case PSI_STREAM_H264: return "H264";
	default: return "*";
	}
}

static void ts_packet(void* ts, int avtype, int64_t pts, int64_t dts, void* data, size_t bytes)
{
	printf("[%s] pts: %08lu, dts: %08lu\n", ts_type(avtype), (unsigned long)pts, (unsigned long)dts);

	mpeg_ts_write(ts, avtype, pts, dts, data, bytes);
}

static void mpeg_ts_file(const char* file, void* ts)
{
	unsigned char ptr[188];
	FILE* fp = fopen(file, "rb");
	while (1 == fread(ptr, sizeof(ptr), 1, fp))
	{
		mpeg_ts_packet_dec(ptr, sizeof(ptr), ts_packet, ts);
	}
	fclose(fp);
}

//mpeg_ts_test("test/fileSequence0.ts", "test/apple.ts")
void mpeg_ts_test(const char* input, const char* output)
{
	struct mpeg_ts_func_t tshandler;
	tshandler.alloc = ts_alloc;
	tshandler.write = ts_write;
	tshandler.free = ts_free;
	
	FILE* fp = fopen(output, "wb");
	void* ts = mpeg_ts_create(&tshandler, fp);

	mpeg_ts_file(input, ts);
	
	mpeg_ts_destroy(ts);
	fclose(fp);
}
