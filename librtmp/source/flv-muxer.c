#include "flv-muxer.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <memory.h>
#include "mpeg4-aac.h"
#include "mpeg4-avc.h"
#include "byte-order.h"
#include "h264-util.h"
#include "h264-nal.h"

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

int flv_muxer_audio(void* p, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	int n, m;
	struct flv_muxer_t* flv;
	flv = (struct flv_muxer_t*)p;
	dts = (0 == dts || UINT32_MAX == dts) ? pts : dts;

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

		flv->ptr[0] = (10 << 4) /* AAC */ | (3 << 2) /* SoundRate */ | (1 << 1) /* 16-bit samples */ | 1 /* Stereo sound */;
		flv->ptr[1] = 0; // AACPacketType: 0-AudioSpecificConfig(AAC sequence header)
		m = mpeg4_aac_audio_specific_config_save(&flv->aac, flv->ptr + 2, flv->capacity - 2);
		assert(m + 2 <= (int)flv->capacity);
		flv->handler(flv->param, FLV_TYPE_AUDIO, flv->ptr, m + 2, dts);
	}

	flv->ptr[0] = (10 << 4) /* AAC */ | (3 << 2) /* 44k-SoundRate */ | (1 << 1) /* 16-bit samples */ | 1 /* Stereo sound */;
	flv->ptr[1] = 1; // AACPacketType: 1-AAC raw
	memcpy(flv->ptr + 2, (uint8_t*)data + n, bytes - n); // AAC exclude ADTS
	assert(bytes - n + 2 <= (int)flv->capacity);
	flv->handler(flv->param, FLV_TYPE_AUDIO, flv->ptr, bytes - n + 2, dts);
	return 0;
}

static void flv_h264_handler(void* param, const void* nalu, size_t bytes)
{
	struct mpeg4_avc_t* avc;
	struct flv_muxer_t* flv;
	int type = ((unsigned char*)nalu)[0] & 0x1f;

	flv = (struct flv_muxer_t*)param;
	avc = &flv->avc;

	if (H264_NAL_SPS == type)
	{
		assert(bytes <= sizeof(avc->sps[avc->nb_sps].data));
		avc->sps[avc->nb_sps].bytes = (uint16_t)bytes;
		memcpy(avc->sps[avc->nb_sps].data, nalu, bytes);
		++avc->nb_sps;
	}
	else if (H264_NAL_PPS == type)
	{
		assert(bytes <= sizeof(avc->pps[avc->nb_pps].data));
		avc->pps[avc->nb_pps].bytes = (uint16_t)bytes;
		memcpy(avc->pps[avc->nb_pps].data, nalu, bytes);
		++avc->nb_pps;
	}
	//else if (H264_NAL_SPS_EXTENSION == type || H264_NAL_SPS_SUBSET == type)
	//{
	//}
	else if (H264_NAL_IDR == type)
	{
		flv->keyframe = 1;
	}
	//	else
	{
		be_write_uint32(flv->ptr + flv->bytes, bytes);
		memcpy(flv->ptr + flv->bytes + 4, nalu, bytes);
		flv->bytes += bytes + 4;
	}
}

int flv_muxer_video(void* p, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	int m, compositionTime;
	struct flv_muxer_t* flv;
	flv = (struct flv_muxer_t*)p;

	if (flv->capacity < bytes + 2048/*AVCDecoderConfigurationRecord*/)
	{
		if (0 != flv_muxer_alloc(flv, bytes + 2048))
			return ENOMEM;
	}

	flv->bytes = 5;
	flv->keyframe = 0;
	flv->avc.nb_sps = 0;
	flv->avc.nb_pps = 0;
	h264_stream(data, bytes, flv_h264_handler, flv);

	if (0 == flv->video)
	{
		if (flv->avc.nb_sps < 1 || flv->avc.sps[0].bytes < 4)
			return 0;

		flv->avc.profile = flv->avc.sps[0].data[1];
		flv->avc.compatibility = flv->avc.sps[0].data[2];
		flv->avc.level = flv->avc.sps[0].data[3];
		flv->avc.nalu = 4;

		flv->ptr[flv->bytes + 0] = ((flv->keyframe ? 1 : 2) << 4) /* FrameType */ | 7 /* AVC */;
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
		flv->ptr[0] = ((flv->keyframe ? 1 : 2) << 4) /* FrameType */ | 7 /* AVC */;
		flv->ptr[1] = 1; // AVC NALU
		flv->ptr[2] = (compositionTime >> 16) & 0xFF;
		flv->ptr[3] = (compositionTime >> 8) & 0xFF;
		flv->ptr[4] = compositionTime & 0xFF;

		assert(flv->bytes + 5 <= (int)flv->capacity);
		flv->handler(flv->param, FLV_TYPE_VIDEO, flv->ptr, flv->bytes + 5, dts);
	}
	return 0;
}

#if defined(_DEBUG) || defined(DEBUG)
//static void flv_muxer_test_handler()
//{
//}
//void flv_muxer_test(void)
//{
//	void* muxer;
//	muxer = flv_muxer_create(flv_muxer_test_handler, NULL);
//	flv_muxer_audio(muxer, );
//	flv_muxer_video(muxer, );
//	flv_muxer_audio(muxer, );
//	flv_muxer_audio(muxer, );
//	flv_muxer_destroy(muxer);
//}
#endif
