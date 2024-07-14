#include "mkv-internal.h"
#include "mkv-format.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

const static struct 
{
	const char* name;
	enum mkv_codec_t codec;
	int prefix; // compare prefix
} s_tags[] = 
{
	{ "V_MS/VFW/FOURCC",	MKV_CODEC_VIDEO_VCM, -1 },
	{ "V_UNCOMPRESSED",		MKV_CODEC_VIDEO_UNCOMPRESSED, -1 },
	{ "V_MPEG4/ISO/SP",		MKV_CODEC_VIDEO_DIVX4, -1 },
	{ "V_MPEG4/ISO/ASP",	MKV_CODEC_VIDEO_DIVX5, -1 },
	{ "V_MPEG4/ISO/AP",		MKV_CODEC_VIDEO_MPEG4, -1 },
	{ "V_MPEG4/MS/V3",		MKV_CODEC_VIDEO_MPEG4_MSV3, -1 },
	{ "V_MPEG1",			MKV_CODEC_VIDEO_MPEG1, -1 },
	{ "V_MPEG2",			MKV_CODEC_VIDEO_MPEG2, -1 },
	{ "V_MPEG4/ISO/AVC",	MKV_CODEC_VIDEO_H264, -1 },
	{ "V_MPEGH/ISO/HEVC",	MKV_CODEC_VIDEO_H265, -1 },
	{ "V_MPEGI/ISO/VVC",	MKV_CODEC_VIDEO_H266, -1 },
	{ "V_REAL/RV10",		MKV_CODEC_VIDEO_RV10, -1 },
	{ "V_REAL/RV20",		MKV_CODEC_VIDEO_RV20, -1 },
	{ "V_REAL/RV30",		MKV_CODEC_VIDEO_RV30, -1 },
	{ "V_REAL/RV40",		MKV_CODEC_VIDEO_RV40, -1 },
	{ "V_QUICKTIME",		MKV_CODEC_VIDEO_QUICKTIME, -1 },
	{ "V_THEORA",			MKV_CODEC_VIDEO_THEORA, -1 },
	{ "V_PRORES",			MKV_CODEC_VIDEO_PRORES, -1 },
	{ "V_VP8",				MKV_CODEC_VIDEO_VP8, -1 },
	{ "V_VP9",				MKV_CODEC_VIDEO_VP9, -1 },
	{ "V_FFV1",				MKV_CODEC_VIDEO_FFV1, -1 },
	{ "V_AV1",				MKV_CODEC_VIDEO_AV1, -1 },
	{ "V_DIRAC",			MKV_CODEC_VIDEO_DIRAC, -1 },
	{ "V_MJPEG",			MKV_CODEC_VIDEO_MJPEG, -1 },

	{ "A_MPEG/L3",			MKV_CODEC_AUDIO_MP3, -1 },
	{ "A_MPEG/L2",			MKV_CODEC_AUDIO_MP2, -1 },
	{ "A_MPEG/L1",			MKV_CODEC_AUDIO_MP1, -1 },
	{ "A_PCM/INT/BIG",		MKV_CODEC_AUDIO_PCM_BE, -1 },
	{ "A_PCM/INT/LIT",		MKV_CODEC_AUDIO_PCM_LE, -1 },
	{ "A_PCM/FLOAT/IEEE",	MKV_CODEC_AUDIO_PCM_FLOAT, -1 },
	{ "A_MPC",				MKV_CODEC_AUDIO_MPC, -1 },
	{ "A_AC3",				MKV_CODEC_AUDIO_AC3, 5 },
	{ "A_ALAC",				MKV_CODEC_AUDIO_ALAC, -1 },
	{ "A_DTS",				MKV_CODEC_AUDIO_DTS, 5 },
	{ "A_VORBIS",			MKV_CODEC_AUDIO_VORBIS, -1 },
	{ "A_FLAC",				MKV_CODEC_AUDIO_FLAC, -1 },
	{ "A_REAL/14_4",		MKV_CODEC_AUDIO_RA1, -1 },
	{ "A_REAL/28_8",		MKV_CODEC_AUDIO_RA2, -1 },
	{ "A_REAL/COOK",		MKV_CODEC_AUDIO_COOK, -1 },
	{ "A_REAL/SIPR",		MKV_CODEC_AUDIO_SIPR, -1 },
	{ "A_REAL/RALF",		MKV_CODEC_AUDIO_RALF, -1 },
	{ "A_REAL/ATRC",		MKV_CODEC_AUDIO_ATRC, -1 },
	{ "A_MS/ACM",			MKV_CODEC_AUDIO_ACM, -1 },
	{ "A_AAC/MPEG4/LC",		MKV_CODEC_AUDIO_AAC, 5 },
	{ "A_QUICKTIME",		MKV_CODEC_AUDIO_QUICKTIME, 12 },
	{ "A_TTA1",				MKV_CODEC_AUDIO_TTA1, -1 },
	{ "A_WAVPACK4",			MKV_CODEC_AUDIO_WAVPACK4, -1 },
	{ "A_OPUS",				MKV_CODEC_AUDIO_OPUS, -1 },
	{ "A_TRUEHD",			MKV_CODEC_AUDIO_TRUEHD, -1 },
	{ "A_EAC3",				MKV_CODEC_AUDIO_EAC3, -1 },

	{ "S_TEXT/UTF8",		MKV_CODEC_SUBTITLE_TEXT, -1 },
	{ "S_TEXT/SSA",			MKV_CODEC_SUBTITLE_SSA, -1 },
	{ "S_TEXT/ASS",			MKV_CODEC_SUBTITLE_ASS, -1 },
	{ "S_ASS",				MKV_CODEC_SUBTITLE_ASS, -1 },
	{ "S_SSA",				MKV_CODEC_SUBTITLE_SSA, -1 },
	{ "S_TEXT/USF",			MKV_CODEC_SUBTITLE_USF, -1 },
	{ "S_TEXT/WEBVTT",		MKV_CODEC_SUBTITLE_WEBVTT, -1 },
	{ "S_IMAGE/BMP",		MKV_CODEC_SUBTITLE_BMP, -1 },
	{ "S_DVBSUB",			MKV_CODEC_SUBTITLE_DVBSUB, -1 },
	{ "S_VOBSUB",			MKV_CODEC_SUBTITLE_VOBSUB, -1 },
	{ "S_HDMV/PGS",			MKV_CODEC_SUBTITLE_PGS, -1 },
	{ "S_HDMV/TEXTST",		MKV_CODEC_SUBTITLE_TEXTST, -1 },
	{ "S_KATE",				MKV_CODEC_SUBTITLE_KATE, -1 },
};

enum mkv_codec_t mkv_codec_find_id(const char* name)
{
	int i;
	size_t n;

	name = name ? name : "";
	n = strlen(name);

	for (i = 0; i < sizeof(s_tags) / sizeof(s_tags[0]); i++)
	{
		if ((-1 == s_tags[i].prefix && 0 == strcmp(s_tags[i].name, name))
			|| (s_tags[i].prefix > 0 && 0 == strncmp(s_tags[i].name, name, s_tags[i].prefix) && ((int)n == s_tags[i].prefix || name[s_tags[i].prefix] == '/')))
			return s_tags[i].codec;
	}

	return MKV_CODEC_UNKNOWN;
}

const char* mkv_codec_find_name(enum mkv_codec_t codec)
{
	int i;
	for (i = 0; i < sizeof(s_tags) / sizeof(s_tags[0]); i++)
	{
		if (s_tags[i].codec == codec)
			return s_tags[i].name;
	}

	return NULL;
}

int mkv_codec_is_video(enum mkv_codec_t codec)
{
	return codec > 0 && codec < 0x1000;
}

int mkv_codec_is_audio(enum mkv_codec_t codec)
{
	return codec >= 0x1000 && codec < 0x2000;
}

int mkv_codec_is_subtitle(enum mkv_codec_t codec)
{
	return codec >= 0x2000 && codec < 0x3000;
}
