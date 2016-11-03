#include "rtmp-client.h"
#include "h264-util.h"
#include "h264-nal.h"
#include <assert.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include "mpeg4-avc.h"

static void rtmp_client_sps_handler(void* param, const void* nalu, size_t bytes)
{
	struct mpeg4_avc_t* avc = (struct mpeg4_avc_t*)param;
	int type = ((unsigned char*)nalu)[0] & 0x1f;

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
}

size_t rtmp_client_make_AVCDecoderConfigurationRecord(const void* video, size_t bytes, void* out, size_t osize)
{
	int r;
	struct mpeg4_avc_t avc;

	memset(&avc, 0, sizeof(avc));
	h264_stream(video, bytes, rtmp_client_sps_handler, &avc);
	if (avc.nb_sps < 1)
		return 0;

	avc.profile = avc.sps[0].data[1];
	avc.compatibility = avc.sps[0].data[2];
	avc.level = avc.sps[0].data[3];
	avc.nalu = 4;

	if (osize < (size_t)(avc.nb_sps + avc.nb_pps) * 66 + 7)
		return 0;

	r = mpeg4_avc_decoder_configuration_record_save(&avc, (uint8_t*)out, osize);
	return r < 0 ? 0 : (size_t)r;
}
