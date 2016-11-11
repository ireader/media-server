#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
#include <assert.h>
#include <stdio.h>
#include <list>
#include <memory.h>

struct mpeg_ts_test_t
{
	struct audio
	{
		std::auto_ptr<char> data;
		size_t bytes;
		int64_t pts;
		int64_t dts;
		audio(std::auto_ptr<char>& ptr, size_t bytes, int64_t pts, int64_t dts)
			:data(ptr), bytes(bytes), pts(pts), dts(dts)
		{
		}
	};

	std::list<audio> audios;
	void* ts;
};

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

extern  "C" size_t mpeg_ts_h264(void* h264, size_t bytes);

static void ts_packet(void* param, int avtype, int64_t pts, int64_t dts, void* data, size_t bytes)
{
	struct mpeg_ts_test_t* ctx = (struct mpeg_ts_test_t*)param;
	char s_char[] = { ' ', 'A', 'V', 'a', 'v' };
	printf("[%c] pts: %08lu, dts: %08lu\n", s_char[avtype], (unsigned long)pts, (unsigned long)dts);

	if (PSI_STREAM_AAC == avtype)
	{
		std::auto_ptr<char> ptr(new char[bytes]);
		memcpy(ptr.get(), data, bytes);
		ctx->audios.push_back(mpeg_ts_test_t::audio(ptr, bytes, pts, dts));
	}
	else if (PSI_STREAM_H264 == avtype)
	{
		while (!ctx->audios.empty())
		{
			const mpeg_ts_test_t::audio& audio = ctx->audios.front();
			if (audio.dts > dts)
				break;

			mpeg_ts_write(ctx->ts, PSI_STREAM_AAC, audio.pts, audio.dts, audio.data.get(), audio.bytes);
			ctx->audios.pop_front();
		}

		mpeg_ts_write(ctx->ts, avtype, pts, dts, data, bytes);
	}
	else
	{
		assert(0);
	}
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

//mpeg_ts_test("test/apple.ts", "test/fileSequence0.ts")
void mpeg_ts_test(const char* input, const char* output)
{
	struct mpeg_ts_test_t ctx;

	struct mpeg_ts_func_t tshandler;
	tshandler.alloc = ts_alloc;
	tshandler.write = ts_write;
	tshandler.free = ts_free;
	
	FILE* fp = fopen(output, "wb");
	ctx.ts = mpeg_ts_create(&tshandler, fp);

	mpeg_ts_file(input, &ctx);
	
	mpeg_ts_destroy(ctx.ts);
	fclose(fp);
}
