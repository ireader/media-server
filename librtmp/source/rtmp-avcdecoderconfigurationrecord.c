#include "rtmp-client.h"
#include "h264-nalu.h"
#include "h264-sps.h"
#include <assert.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>

struct H264Context
{
	uint8_t sps[1024];
	uint8_t pps[1024];
	uint32_t spsOffset;
	uint32_t ppsOffset;
	uint8_t spsCount;
	uint8_t ppsCount;
};

static void rtmp_write_int16(uint8_t* p, uint32_t bytes)
{
	p[0] = (bytes >> 8) & 0xFF;
	p[1] = bytes & 0xFF;
}

static void rtmp_client_sps_handler(void* param, const unsigned char* nalu, unsigned int bytes)
{
	struct H264Context* ctx = (struct H264Context*)param;
	int type = nalu[0] & 0x1f;

	if (H264_NALU_SPS == type)
	{
		assert(ctx->spsOffset + bytes + 2 <= sizeof(ctx->sps));
		if (ctx->spsOffset + bytes + 2 <= sizeof(ctx->sps))
		{
			rtmp_write_int16(ctx->sps + ctx->spsOffset, bytes);
			memcpy(ctx->sps + ctx->spsOffset + 2, nalu, bytes);
			ctx->spsOffset += 2 + bytes;
			++ctx->spsCount;
		}
	}
	else if (H264_NALU_PPS == type)
	{
		assert(ctx->ppsOffset + bytes + 2 <= sizeof(ctx->pps));
		if (ctx->ppsOffset + bytes + 2 <= sizeof(ctx->pps))
		{
			rtmp_write_int16(ctx->pps + ctx->ppsOffset, bytes);
			memcpy(ctx->pps + ctx->ppsOffset + 2, nalu, bytes);
			ctx->ppsOffset += 2 + bytes;
			++ctx->ppsCount;
		}
	}
	//else if (H264_NALU_SPS_EXTENSION == type || H264_NALU_SPS_SUBSET == type)
	//{
	//}
}

int rtmp_client_make_AVCDecoderConfigurationRecord(void* out, const void* video, unsigned int bytes)
{
	int i;
	struct h264_sps_t sps;
	struct H264Context ctx;
	uint8_t *p = (uint8_t*)out;

	memset(&ctx, 0, sizeof(struct H264Context));
	h264_nalu((const unsigned char*)video, bytes, rtmp_client_sps_handler, &ctx);
	if (ctx.spsCount < 0)
	{
		printf("video sequence don't have SPS/PPS NALU\n");
		return -1;
	}

	memset(&sps, 0, sizeof(struct h264_sps_t));
	h264_parse_sps(ctx.sps + 2, (ctx.sps[0] << 8) | ctx.sps[1], &sps);

	// AVCDecoderConfigurationRecord
	// ISO/IEC 14496-15:2010
	// 5.2.4.1.1 Syntax
	p[0] = 1; // configurationVersion
	p[1] = sps.profile_idc; // AVCProfileIndication
	p[2] = sps.constraint_set_flag; // profile_compatibility
	p[3] = sps.level_idc; // AVCLevelIndication
	p[4] = 0xFF; // lengthSizeMinusOne: 3
	i = 5;

	// sps
	p[i++] = 0xE0 | ctx.spsCount;
	memcpy(p + i, ctx.sps, ctx.spsOffset);
	i += ctx.spsOffset;

	// pps
	p[i++] = ctx.ppsCount;
	memcpy(p + i, ctx.pps, ctx.ppsOffset);
	i += ctx.ppsOffset;

	if (sps.profile_idc == 100 || sps.profile_idc == 110 ||
		sps.profile_idc == 122 || sps.profile_idc == 244 || sps.profile_idc == 44 ||
		sps.profile_idc == 83 || sps.profile_idc == 86 || sps.profile_idc == 118 ||
		sps.profile_idc == 128 || sps.profile_idc == 138 || sps.profile_idc == 139 ||
		sps.profile_idc == 134)
	{
		p[i++] = 0xFC | sps.chroma_format_idc;
		p[i++] = 0xF8 | sps.chroma.bit_depth_luma_minus8;
		p[i++] = 0xF8 | sps.chroma.bit_depth_chroma_minus8;
		p[i++] = 0; // numOfSequenceParameterSetExt
	}

	return i;
}
