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

struct flv_muxer_t
{
	flv_muxer_handler handler;
	void* param;

	uint8_t aac_sequence_header;
	uint8_t avc_sequence_header;

	struct mpeg4_aac_t aac;
	struct mpeg4_avc_t avc;
	int keyframe;

	uint8_t* ptr;
	size_t bytes;
	size_t capacity;
};

struct flv_muxer_t* flv_muxer_create(flv_muxer_handler handler, void* param)
{
	struct flv_muxer_t* flv;
	flv = (struct flv_muxer_t*)calloc(1, sizeof(struct flv_muxer_t));
	if (NULL == flv)
		return NULL;

	flv->handler = handler;
	flv->param = param;
	return flv;
}

void flv_muxer_destroy(struct flv_muxer_t* flv)
{
	if (flv->ptr)
	{
		assert(flv->capacity > 0);
		free(flv->ptr);
		flv->ptr = NULL;
	}

	free(flv);
}

int flv_muxer_reset(struct flv_muxer_t* flv)
{
	flv->aac_sequence_header = 0;
	flv->avc_sequence_header = 0;
	return 0;
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

int flv_muxer_mp3(struct flv_muxer_t* flv, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	uint8_t ch, hz;
	struct mp3_header_t mp3;
	(void)pts;

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

	flv->ptr[0] = (FLV_AUDIO_MP3 /*<< 4*/) /* SoundFormat */ | (hz << 2) /* SoundRate */ | (1 << 1) /* 16-bit samples */ | ch /* Stereo sound */;
	memcpy(flv->ptr + 1, data, bytes); // MP3
	return flv->handler(flv->param, FLV_TYPE_AUDIO, flv->ptr, bytes + 1, dts);
}

int flv_muxer_aac(struct flv_muxer_t* flv, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	int r, n, m;
	(void)pts;

	if (flv->capacity < bytes + 2/*AudioTagHeader*/ + 2/*AudioSpecificConfig*/)
	{
		if (0 != flv_muxer_alloc(flv, bytes + 4))
			return ENOMEM;
	}

	/* ADTS */
	n = mpeg4_aac_adts_load(data, bytes, &flv->aac);
	if (n <= 0)
		return -1; // invalid data

	if (0 == flv->aac_sequence_header)
	{
		flv->aac_sequence_header = 1; // once only

		flv->ptr[0] = (FLV_AUDIO_AAC /*<< 4*/) /* SoundFormat */ | (3 << 2) /* 44k-SoundRate */ | (1 << 1) /* 16-bit samples */ | 1 /* Stereo sound */;
		flv->ptr[1] = 0; // AACPacketType: 0-AudioSpecificConfig(AAC sequence header)
		m = mpeg4_aac_audio_specific_config_save(&flv->aac, flv->ptr + 2, flv->capacity - 2);
		assert(m + 2 <= (int)flv->capacity);
		r = flv->handler(flv->param, FLV_TYPE_AUDIO, flv->ptr, m + 2, dts);
		if (0 != r) return r;
	}

	flv->ptr[0] = (FLV_AUDIO_AAC /*<< 4*/) /* SoundFormat */ | (3 << 2) /* 44k-SoundRate */ | (1 << 1) /* 16-bit samples */ | 1 /* Stereo sound */;
	flv->ptr[1] = 1; // AACPacketType: 1-AAC raw
	memcpy(flv->ptr + 2, (uint8_t*)data + n, bytes - n); // AAC exclude ADTS
	assert(bytes - n + 2 <= (int)flv->capacity);
	return flv->handler(flv->param, FLV_TYPE_AUDIO, flv->ptr, bytes - n + 2, dts);
}

static int flv_muxer_h264(struct flv_muxer_t* flv, uint32_t pts, uint32_t dts)
{
	int r;
	int m, compositionTime;

	if (0 == flv->avc_sequence_header)
	{
		if (flv->avc.nb_sps < 1 || flv->avc.nb_pps < 1)
			return 0;

		flv->ptr[flv->bytes + 0] = (1 << 4) /*FrameType*/ | FLV_VIDEO_H264 /*CodecID*/;
		flv->ptr[flv->bytes + 1] = 0; // AVC sequence header
		flv->ptr[flv->bytes + 2] = 0; // CompositionTime 0
		flv->ptr[flv->bytes + 3] = 0;
		flv->ptr[flv->bytes + 4] = 0;
		m = mpeg4_avc_decoder_configuration_record_save(&flv->avc, flv->ptr + flv->bytes + 5, flv->capacity - flv->bytes - 5);
		if (m <= 0)
			return -1; // invalid data

		flv->avc_sequence_header = 1; // once only
		assert(flv->bytes + m + 5 <= (int)flv->capacity);
		r = flv->handler(flv->param, FLV_TYPE_VIDEO, flv->ptr + flv->bytes, m + 5, dts);
		if (0 != r) return r;
	}

	// has video frame
	if (flv->bytes > 5)
	{
		compositionTime = pts - dts;
		flv->ptr[0] = ((flv->keyframe ? 1 : 2) << 4) /*FrameType*/ | FLV_VIDEO_H264 /*CodecID*/;
		flv->ptr[1] = 1; // AVC NALU
		flv->ptr[2] = (compositionTime >> 16) & 0xFF;
		flv->ptr[3] = (compositionTime >> 8) & 0xFF;
		flv->ptr[4] = compositionTime & 0xFF;

		assert(flv->bytes <= (int)flv->capacity);
		return flv->handler(flv->param, FLV_TYPE_VIDEO, flv->ptr, flv->bytes, dts);
	}
	return 0;
}

int flv_muxer_avc(struct flv_muxer_t* flv, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	if (flv->capacity < bytes + 2048/*AVCDecoderConfigurationRecord*/)
	{
		if (0 != flv_muxer_alloc(flv, bytes + 2048))
			return ENOMEM;
	}

	flv->bytes = 5;
	flv->bytes += mpeg4_annexbtomp4(&flv->avc, data, bytes, flv->ptr + flv->bytes, flv->capacity - flv->bytes);
	if (flv->bytes <= 5)
		return ENOMEM;

	flv->keyframe = flv->avc.chroma_format_idc; // hack
	return flv_muxer_h264(flv, pts, dts);
}

int flv_muxer_h264_nalu(struct flv_muxer_t* flv, const void* nalu, size_t bytes, uint32_t pts, uint32_t dts)
{
	int r;
	uint8_t type = (*(const uint8_t*)nalu) & 0x1f;

	switch (type)
	{
	case 7:
		// FIXME: check sps id
		if (bytes > sizeof(flv->avc.sps[0].data)
			|| flv->avc.nb_sps >= sizeof(flv->avc.sps) / sizeof(flv->avc.sps[0]))
		{
			assert(0);
			return -1;
		}
		if (flv->avc_sequence_header)
			return 0;
		memcpy(flv->avc.sps[flv->avc.nb_sps].data, nalu, bytes);
		flv->avc.sps[flv->avc.nb_sps].bytes = (uint16_t)bytes;
		flv->avc.nb_sps++;

		flv->avc.nalu = 4;
		flv->avc.profile = flv->avc.sps[0].data[1];
		flv->avc.compatibility = flv->avc.sps[0].data[2];
		flv->avc.level = flv->avc.sps[0].data[3];
		break;

	case 8:
		// FIXME: check pps/sps id
		if (bytes > sizeof(flv->avc.pps[0].data)
			|| (int)flv->avc.nb_pps >= sizeof(flv->avc.pps) / sizeof(flv->avc.pps[0]))
		{
			assert(0);
			return -1;
		}
		if (flv->avc_sequence_header)
			return 0;
		memcpy(flv->avc.pps[flv->avc.nb_pps].data, nalu, bytes);
		flv->avc.pps[flv->avc.nb_pps].bytes = (uint16_t)bytes;
		flv->avc.nb_pps++;
		break;

	default:
		if (flv->capacity < bytes + 2048/*AVCDecoderConfigurationRecord*/)
		{
			if (0 != flv_muxer_alloc(flv, bytes + 2048))
				return ENOMEM;
		}

		flv->ptr[flv->bytes + 5] = (uint8_t)((bytes >> 24) & 0xFF);
		flv->ptr[flv->bytes + 6] = (uint8_t)((bytes >> 16) & 0xFF);
		flv->ptr[flv->bytes + 7] = (uint8_t)((bytes >> 8) & 0xFF);
		flv->ptr[flv->bytes + 8] = (uint8_t)((bytes >> 0) & 0xFF);
		memcpy(flv->ptr + 5 + flv->bytes + 4, nalu, bytes);
		flv->bytes += bytes + 4;
		flv->keyframe = type == 5;
	}

	if (type < 1 || type > 5)
		return 0; // no-VCL

	assert(flv->bytes > 0);
	flv->bytes += 5;
	r = flv_muxer_h264(flv, pts, dts);
	flv->bytes = 0;
	return r;
}
