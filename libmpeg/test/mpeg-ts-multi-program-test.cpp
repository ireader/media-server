#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "mov-reader.h"
#include "mov-format.h"
#include "mpeg4-aac.h"
#include "mpeg4-avc.h"
#include "mpeg4-hevc.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <map>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static uint8_t s_packet[2 * 1024 * 1024];
static uint8_t s_buffer[4 * 1024 * 1024];

struct mpeg_ts_multi_program_test_t
{
	int count;
	struct
	{
		int pn;
		int track;
		int stream;
		uint8_t object;

		union
		{
			mpeg4_avc_t avc;
			mpeg4_hevc_t hevc;
		} v;

		union
		{
			mpeg4_aac_t aac;
		} a;
	} pn[4];
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

static int ts_write(void* param, const void* packet, size_t bytes)
{
	return 1 == fwrite(packet, bytes, 1, (FILE*)param) ? 0 : ferror((FILE*)param);
}

static void onread(void* param, uint32_t track, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	mpeg_ts_multi_program_test_t* ctx = (mpeg_ts_multi_program_test_t*)param;
	for (int i = 0; i < ctx->count; i++)
	{
		if (ctx->pn[i].track == track)
		{
			if (MOV_OBJECT_H264 == ctx->pn[i].object)
			{
				bytes = h264_mp4toannexb(&ctx->pn[i].v.avc, data, bytes, s_packet, sizeof(s_packet));
				data = s_packet;
			}
			else if (MOV_OBJECT_HEVC == ctx->pn[i].object)
			{
				bytes = h265_mp4toannexb(&ctx->pn[i].v.hevc, data, bytes, s_packet, sizeof(s_packet));
				data = s_packet;
			}
			else if (MOV_OBJECT_AAC == ctx->pn[i].object)
			{
				int n = mpeg4_aac_adts_save(&ctx->pn[i].a.aac, bytes, s_packet, sizeof(s_packet));
				memcpy(s_packet + n, data, bytes);
				data = s_packet;
				bytes += n;
			}
			assert(0 == mpeg_ts_write(ctx->ts, ctx->pn[i].stream, (flags & MOV_AV_FLAG_KEYFREAME) ? 1 : 0, pts * 90, dts * 90, data, bytes));
			return;
		}
	}
}

static void mov_video_info(void* param, uint32_t track, uint8_t object, int /*width*/, int /*height*/, const void* extra, size_t bytes)
{
	assert(MOV_OBJECT_H264 == object || MOV_OBJECT_HEVC == object);
	mpeg_ts_multi_program_test_t* ctx = (mpeg_ts_multi_program_test_t*)param;
	ctx->pn[ctx->count].pn = ctx->count + 1;
	ctx->pn[ctx->count].track = track;
	ctx->pn[ctx->count].object = object;
	assert(0 == mpeg_ts_add_program(ctx->ts, ctx->pn[ctx->count].pn, NULL, 0));

	switch (object)
	{
	case MOV_OBJECT_H264:
		assert(mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &ctx->pn[ctx->count].v.avc) > 0);
		ctx->pn[ctx->count].stream = mpeg_ts_add_program_stream(ctx->ts, ctx->pn[ctx->count].pn, PSI_STREAM_H264, NULL, 0);
		break;
	case MOV_OBJECT_HEVC:
		assert(mpeg4_hevc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &ctx->pn[ctx->count].v.hevc) > 0);
		ctx->pn[ctx->count].stream = mpeg_ts_add_program_stream(ctx->ts, ctx->pn[ctx->count].pn, PSI_STREAM_H265, NULL, 0);
		break;
	default:
		assert(0);
	}
	ctx->count++;
}

static void mov_audio_info(void* param, uint32_t track, uint8_t object, int channel_count, int /*bit_per_sample*/, int sample_rate, const void* extra, size_t bytes)
{
	assert(MOV_OBJECT_AAC == object || MOV_OBJECT_OPUS == object);
	mpeg_ts_multi_program_test_t* ctx = (mpeg_ts_multi_program_test_t*)param;
	ctx->pn[ctx->count].pn = ctx->count + 1;
	ctx->pn[ctx->count].track = track;
	ctx->pn[ctx->count].object = object;
	assert(0 == mpeg_ts_add_program(ctx->ts, ctx->pn[ctx->count].pn, NULL, 0));
	assert(mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &ctx->pn[ctx->count].a.aac) > 0);
	ctx->pn[ctx->count].stream = mpeg_ts_add_program_stream(ctx->ts, ctx->pn[ctx->count].pn, MOV_OBJECT_AAC == object ? PSI_STREAM_AAC : PSI_STREAM_AUDIO_OPUS, NULL, 0);
	ctx->count++;
}

void mpeg_ts_multi_program_test(const char* mp4)
{
	struct mpeg_ts_multi_program_test_t ctx;
	memset(&ctx, 0, sizeof(ctx));

	char output[256] = { 0 };
	snprintf(output, sizeof(output) - 1, "%s.ts", mp4);

	struct mpeg_ts_func_t tshandler;
	tshandler.alloc = ts_alloc;
	tshandler.write = ts_write;
	tshandler.free = ts_free;

	FILE* fp = fopen(output, "wb");
	ctx.ts = mpeg_ts_create(&tshandler, fp);

	FILE* rfp = fopen(mp4, "rb");
	mov_reader_t* mov = mov_reader_create(mov_file_buffer(), rfp);

	struct mov_reader_trackinfo_t info = { mov_video_info, mov_audio_info };
	mov_reader_getinfo(mov, &info, &ctx);

	while (mov_reader_read(mov, s_buffer, sizeof(s_buffer), onread, &ctx) > 0)
	{
	}

	mov_reader_destroy(mov);
	mpeg_ts_destroy(ctx.ts);
	fclose(rfp);
	fclose(fp);
}
