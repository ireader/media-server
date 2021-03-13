#include "mov-internal.h"
#include <stdint.h>

struct mov_object_tag {
	uint8_t id;
	uint32_t tag;
};

static struct mov_object_tag s_tags[] = {
	{ MOV_OBJECT_H264,	MOV_H264 }, // AVCSampleEntry  (ISO/IEC 14496-15:2010)
	{ MOV_OBJECT_H264,	MOV_TAG('a', 'v', 'c', '2') }, // AVC2SampleEntry (ISO/IEC 14496-15:2010)
    { MOV_OBJECT_H264,	MOV_TAG('a', 'v', 'c', '3') }, // AVCSampleEntry (ISO/IEC 14496-15:2017)
    { MOV_OBJECT_H264,	MOV_TAG('a', 'v', 'c', '4') }, // AVC2SampleEntry (ISO/IEC 14496-15:2017)
	{ MOV_OBJECT_HEVC,	MOV_HEVC }, // HEVCSampleEntry (ISO/IEC 14496-15:2013)
	{ MOV_OBJECT_HEVC,	MOV_TAG('h', 'e', 'v', '1') }, // HEVCSampleEntry (ISO/IEC 14496-15:2013)
	{ MOV_OBJECT_MP4V,	MOV_MP4V },
	{ MOV_OBJECT_AAC,	MOV_MP4A },
	{ MOV_OBJECT_MP3,	MOV_MP4A }, // mp4_read_decoder_config_descriptor
	{ MOV_OBJECT_MP1A,	MOV_MP4A }, // mp4_read_decoder_config_descriptor
	{ MOV_OBJECT_G711a,	MOV_TAG('a', 'l', 'a', 'w') },
	{ MOV_OBJECT_G711u,	MOV_TAG('u', 'l', 'a', 'w') },
	{ MOV_OBJECT_TEXT,	MOV_TAG('t', 'x', '3', 'g') },
	{ MOV_OBJECT_TEXT,	MOV_TAG('t', 'e', 'x', 't') },
	{ MOV_OBJECT_TEXT,	MOV_TAG('c', '6', '0', '8') },
    { MOV_OBJECT_OPUS,  MOV_OPUS },
    { MOV_OBJECT_VP8,   MOV_VP8 },
    { MOV_OBJECT_VP9,   MOV_VP9 },
    { MOV_OBJECT_AV1,	MOV_AV1 },
    { MOV_OBJECT_AC3,   MOV_AC3 },
    { MOV_OBJECT_EAC3,  MOV_TAG('e', 'c', '-', '3') },
    { MOV_OBJECT_DTS,   MOV_DTS },
    { MOV_OBJECT_VC1,   MOV_VC1 },
    { MOV_OBJECT_DIRAC, MOV_DIRAC },
};

uint32_t mov_object_to_tag(uint8_t object)
{
	int i;
	for (i = 0; i < sizeof(s_tags) / sizeof(s_tags[0]); i++)
	{
		if (s_tags[i].id == object)
			return s_tags[i].tag;
	}
	return 0;
}

uint8_t mov_tag_to_object(uint32_t tag)
{
	int i;
	for (i = 0; i < sizeof(s_tags) / sizeof(s_tags[0]); i++)
	{
		if (s_tags[i].tag == tag)
			return s_tags[i].id;
	}
	return 0;
}
