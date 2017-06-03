#include "flv-muxer.h"
#include "flv-proto.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "mpeg4-aac.h"
#include "mpeg4-avc.h"
#include "mp3-header.h"

#define FLV_TYPE_AUDIO 8
#define FLV_TYPE_VIDEO 9
#define FLV_TYPE_SCRIPT 18

struct flv_muxer_t
{
	flv_muxer_handler handler;
	void* param;

	uint8_t audio;
	uint8_t video;

	struct mpeg4_aac_t aac;
	struct mpeg4_avc_t avc;
	int keyframe;

	uint8_t* ptr;
	size_t bytes;
	size_t capacity;
};

void* flv_muxer_create(flv_muxer_handler handler, void* param)
{
	struct flv_muxer_t* flv;
	flv = (struct flv_muxer_t*)malloc(sizeof(*flv));
	if (NULL == flv)
		return NULL;

	memset(flv, 0, sizeof(*flv));
	flv->handler = handler;
	flv->param = param;
	return flv;
}

void flv_muxer_destroy(void* p)
{
	struct flv_muxer_t* flv;
	flv = (struct flv_muxer_t*)p;

	if (flv->ptr)
	{
		assert(flv->capacity > 0);
		free(flv->ptr);
		flv->ptr = NULL;
	}

	free(flv);
}

static int flv_muxer_alloc(struct flv_muxer_t* flv, size_t bytes)
{
	void* p;
	p = realloc(flv->ptr, bytes);
	if (!p)
		return ENOMEM;

	flv->ptr = (uint8_t*)p;
	flv->capacity = bytes;
	return 0;
}

int flv_muxer_mp3(void* p, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	uint8_t ch, hz;
	struct flv_muxer_t* flv;
	struct mp3_header_t mp3;
	flv = (struct flv_muxer_t*)p;

	if (0 == mp3_header_load(&mp3, data, bytes))
	{
		return EINVAL;
	}
	else
	{
		ch = 3 == mp3.mode ? 0 : 1;
		switch (mp3_get_frequency(&mp3))
		{
		case 5500: hz = 0; break;
		case 11000: hz = 1; break;
		case 22000: hz = 2; break;
		case 44100: hz = 3; break;
		default: hz = 3;
		}
	}

	if (flv->capacity < bytes + 1)
	{
		if (0 != flv_muxer_alloc(flv, bytes + 4))
			return ENOMEM;
	}

	flv->ptr[0] = (FLV_AUDIO_MP3 << 4) /* SoundFormat */ | (hz << 2) /* SoundRate */ | (1 << 1) /* 16-bit samples */ | ch /* Stereo sound */;
	memcpy(flv->ptr + 1, data, bytes); // MP3
	flv->handler(flv->param, FLV_TYPE_AUDIO, flv->ptr, bytes + 1, dts);
	return 0;
}

int flv_muxer_aac(void* p, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	int n, m;
	struct flv_muxer_t* flv;
	flv = (struct flv_muxer_t*)p;

	if (flv->capacity < bytes + 2/*AudioTagHeader*/ + 2/*AudioSpecificConfig*/)
	{
		if (0 != flv_muxer_alloc(flv, bytes + 4))
			return ENOMEM;
	}

	/* ADTS */
	n = mpeg4_aac_adts_load(data, bytes, &flv->aac);
	if (n <= 0)
		return -1; // invalid data

	if (0 == flv->audio)
	{
		flv->audio = 1; // once only

		flv->ptr[0] = (FLV_AUDIO_AAC << 4) /* SoundFormat */ | (3 << 2) /* 44k-SoundRate */ | (1 << 1) /* 16-bit samples */ | 1 /* Stereo sound */;
		flv->ptr[1] = 0; // AACPacketType: 0-AudioSpecificConfig(AAC sequence header)
		m = mpeg4_aac_audio_specific_config_save(&flv->aac, flv->ptr + 2, flv->capacity - 2);
		assert(m + 2 <= (int)flv->capacity);
		flv->handler(flv->param, FLV_TYPE_AUDIO, flv->ptr, m + 2, dts);
	}

	flv->ptr[0] = (FLV_AUDIO_AAC << 4) /* SoundFormat */ | (3 << 2) /* 44k-SoundRate */ | (1 << 1) /* 16-bit samples */ | 1 /* Stereo sound */;
	flv->ptr[1] = 1; // AACPacketType: 1-AAC raw
	memcpy(flv->ptr + 2, (uint8_t*)data + n, bytes - n); // AAC exclude ADTS
	assert(bytes - n + 2 <= (int)flv->capacity);
	flv->handler(flv->param, FLV_TYPE_AUDIO, flv->ptr, bytes - n + 2, dts);
	return 0;
}

int flv_muxer_avc(void* p, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	int m, compositionTime;
	struct flv_muxer_t* flv;
	flv = (struct flv_muxer_t*)p;

	if (flv->capacity < bytes + 2048/*AVCDecoderConfigurationRecord*/)
	{
		if (0 != flv_muxer_alloc(flv, bytes + 2048))
			return ENOMEM;
	}

	flv->avc.chroma_format_idc = 0;
	flv->bytes = 5;
	flv->bytes += mpeg4_annexbtomp4(&flv->avc, data, bytes, flv->ptr + flv->bytes, flv->capacity - flv->bytes);
	if (flv->bytes <= 5)
		return ENOMEM;

	flv->keyframe = flv->avc.chroma_format_idc; // hack

	if (0 == flv->video)
	{
		if (flv->avc.nb_sps < 1 || flv->avc.sps[0].bytes < 4)
			return 0;

		flv->ptr[flv->bytes + 0] = (1 << 4) /*FrameType*/ | FLV_VIDEO_AVC /*CodecID*/;
		flv->ptr[flv->bytes + 1] = 0; // AVC sequence header
		flv->ptr[flv->bytes + 2] = 0; // CompositionTime 0
		flv->ptr[flv->bytes + 3] = 0;
		flv->ptr[flv->bytes + 4] = 0;
		m = mpeg4_avc_decoder_configuration_record_save(&flv->avc, flv->ptr + flv->bytes + 5, flv->capacity - flv->bytes - 5);
		if (m <= 0)
			return -1; // invalid data

		flv->video = 1; // once only
		assert(flv->bytes + m + 5 <= (int)flv->capacity);
		flv->handler(flv->param, FLV_TYPE_VIDEO, flv->ptr + flv->bytes, m + 5, dts);
	}

	// has video frame
	if (flv->bytes > 5)
	{
		compositionTime = pts - dts;
		flv->ptr[0] = ((flv->keyframe ? 1 : 2) << 4) /*FrameType*/ | FLV_VIDEO_AVC /*CodecID*/;
		flv->ptr[1] = 1; // AVC NALU
		flv->ptr[2] = (compositionTime >> 16) & 0xFF;
		flv->ptr[3] = (compositionTime >> 8) & 0xFF;
		flv->ptr[4] = compositionTime & 0xFF;

		assert(flv->bytes <= (int)flv->capacity);
		flv->handler(flv->param, FLV_TYPE_VIDEO, flv->ptr, flv->bytes, dts);
	}
	return 0;
}
