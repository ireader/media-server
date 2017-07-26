extern "C" 
{
#include "libavformat/avformat.h"
}
#include "hls-m3u8.h"
#include "hls-media.h"
#include "hls-param.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "mpeg-ps.h"

static char s_packet[2 * 1024 * 1024];

static void ffmpeg_init()
{
	avcodec_register_all();
	av_register_all();
	avformat_network_init();
}

static AVFormatContext* ffmpeg_open(const char* url)
{
	int r;
	AVFormatContext* ic;
	AVDictionary* opt = NULL;
	ic = avformat_alloc_context();
	if (NULL == ic)
	{
		printf("%s(%s): avformat_alloc_context failed.\n", __FUNCTION__, url);
		return NULL;
	}

	//if (!av_dict_get(ff->opt, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
	//	av_dict_set(&ff->opt, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
	//	scan_all_pmts_set = 1;
	//}

	r = avformat_open_input(&ic, url, NULL, &opt);
	if (0 != r)
	{
		printf("%s: avformat_open_input(%s) => %d\n", __FUNCTION__, url, r);
		return NULL;
	}

	//if (scan_all_pmts_set)
	//	av_dict_set(&format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

	//ff->ic->probesize = 100 * 1024;
	//ff->ic->max_analyze_duration = 5 * AV_TIME_BASE;

	/* If not enough info to get the stream parameters, we decode the
	first frames to get it. (used in mpeg case for example) */
	r = avformat_find_stream_info(ic, NULL/*&opt*/);
	if (r < 0) {
		printf("%s(%s): could not find codec parameters\n", __FUNCTION__, url);
		return NULL;
	}

	av_dict_free(&opt);
	return ic;
}

static void hls_handler(void* m3u8, const void* data, size_t bytes, int64_t pts, int64_t /*dts*/, int64_t duration)
{
	static int i = 0;
	char name[128] = { 0 };
	snprintf(name, sizeof(name), "%d.ts", i++);
	hls_m3u8_add((hls_m3u8_t*)m3u8, name, pts, duration, 0);

	FILE* fp = fopen(name, "wb");
	fwrite(data, 1, bytes, fp);
	fclose(fp);
}

void mp4_to_ts_test(const char* mp4)
{
	ffmpeg_init();

	AVPacket pkt;
	memset(&pkt, 0, sizeof(pkt));
	//av_init_packet(&pkt);

	AVFormatContext* ic = ffmpeg_open(mp4);
	hls_m3u8_t* m3u = hls_m3u8_create(0);
	hls_media_t* hls = hls_media_create(HLS_DURATION * 1000, hls_handler, m3u);

	struct mpeg4_avc_t avc;
	struct mpeg4_aac_t aac;
	for (int i = 0; i < ic->nb_streams; i++)
	{
		AVStream* st = ic->streams[i];
		if (AVMEDIA_TYPE_VIDEO == st->codecpar->codec_type)
		{
			mpeg4_avc_decoder_configuration_record_load(st->codecpar->extradata, st->codecpar->extradata_size, &avc);
		}
		else if (AVMEDIA_TYPE_AUDIO == st->codecpar->codec_type)
		{
			mpeg4_aac_audio_specific_config_load(st->codecpar->extradata, st->codecpar->extradata_size, &aac);
		}
	}

	int r = av_read_frame(ic, &pkt);
	while (0 == r)
	{
		AVStream* st = ic->streams[pkt.stream_index];
		int64_t pts = (int64_t)(pkt.pts * av_q2d(st->time_base) * 1000);
		int64_t dts = (int64_t)(pkt.dts * av_q2d(st->time_base) * 1000);
		if (AVMEDIA_TYPE_VIDEO == st->codecpar->codec_type)
		{
			//printf("[V] pts: %08lld, dts: %08lld\n", pts, dts);
			r = mpeg4_mp4toannexb(&avc, pkt.data, pkt.size, s_packet, sizeof(s_packet));
			hls_media_input(hls, STREAM_VIDEO_H264, s_packet, r, pts, dts, 0);
		}
		else if (AVMEDIA_TYPE_AUDIO == st->codecpar->codec_type)
		{
			//printf("[A] pts: %08lld, dts: %08lld\n", pts, dts);
			r = mpeg4_aac_adts_save(&aac, pkt.size, (uint8_t*)s_packet, sizeof(s_packet));
			memcpy(s_packet + r, pkt.data, pkt.size);
			hls_media_input(hls, STREAM_AUDIO_AAC, s_packet, r + pkt.size, pts, dts, 0);
		}

		//av_packet_unref(&pkt);
		r = av_read_frame(ic, &pkt);
	}

	// write m3u8 file
	hls_media_input(hls, 0, NULL, 0, 0, 0, 1);
	hls_m3u8_playlist(m3u, 1, s_packet, sizeof(s_packet));
	FILE* fp = fopen("playlist.m3u8", "wb");
	fwrite(s_packet, 1, strlen(s_packet), fp);
	fclose(fp);

	avformat_close_input(&ic);
	avformat_free_context(ic);
	hls_media_destroy(hls);
	hls_m3u8_destroy(m3u);
}
