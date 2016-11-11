#include "flv-writer.h"
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
#define FLV_TYPE_VDIEO 9
#define FLV_TYPE_SCRIPT 18

struct flv_writer_t
{
	FILE* fp;
	uint8_t audio;
	uint8_t video;

	struct mpeg4_aac_t aac;
	struct mpeg4_avc_t avc;
	int keyframe;

	uint8_t* ptr;
	size_t bytes;
	size_t offset;
};

static int flv_write_header(struct flv_writer_t* flv)
{
	uint8_t header[9+4];
	memcpy(header, "FLV", 3); // FLV signature
	header[3] = 0x01; // File version
	header[4] = 0x06; // Type flags (audio & video)
	be_write_uint32(header + 5, 9); // Data offset
	be_write_uint32(header + 9, 0); // PreviousTagSize0(Always 0)

	if (sizeof(header) != fwrite(header, 1, sizeof(header), flv->fp))
		return ferror(flv->fp);
	return 0;
}

static inline void flv_write_tag(uint8_t* tag, uint8_t type, uint32_t bytes, uint32_t timestamp)
{
	// TagType
	tag[0] = type & 0x1F;

	// DataSize
	tag[1] = (bytes >> 16) & 0xFF;
	tag[2] = (bytes >> 8) & 0xFF;
	tag[3] = bytes & 0xFF;

	// Timestamp
	tag[4] = (timestamp >> 16) & 0xFF;
	tag[5] = (timestamp >> 8) & 0xFF;
	tag[6] = (timestamp >> 0) & 0xFF;
	tag[7] = (timestamp >> 24) & 0xFF; // Timestamp Extended

	// StreamID(Always 0)
	tag[8] = 0;
	tag[9] = 0;
	tag[10] = 0;
}

static int flv_write_eos(struct flv_writer_t* flv)
{
	uint8_t header[11 + 5 + 4];
	flv_write_tag(header, FLV_TYPE_VDIEO, 5, 0);
	flv->ptr[11] = (1 << 4) /* FrameType */ | 7 /* AVC */;
	flv->ptr[12] = 2; // AVC end of sequence
	flv->ptr[13] = 0;
	flv->ptr[14] = 0;
	flv->ptr[15] = 0;
	be_write_uint32(header + 16, 16); // TAG size

	if (sizeof(header) != fwrite(header, 1, sizeof(header), flv->fp))
		return ferror(flv->fp);
	return 0;
}

void* flv_writer_create(const char* file)
{
	struct flv_writer_t* flv;
	flv = (struct flv_writer_t*)malloc(sizeof(*flv));
	if (NULL == flv)
		return NULL;

	memset(flv, 0, sizeof(*flv));
	flv->fp = fopen(file, "wb");
	if (!flv->fp || 0 != flv_write_header(flv))
	{
		flv_writer_destroy(flv);
		return NULL;
	}

	return flv;
}

void flv_writer_destroy(void* p)
{
	struct flv_writer_t* flv;
	flv = (struct flv_writer_t*)p;

	if (flv->fp)
	{
		flv_write_eos(flv);
		fclose(flv->fp);
		flv->fp = NULL;
	}

	if (flv->ptr)
	{
		assert(flv->bytes > 0);
		free(flv->ptr);
		flv->ptr = NULL;
	}

	free(flv);
}

static inline int flv_write_alloc(struct flv_writer_t* flv, size_t bytes)
{
	void* p;
	p = realloc(flv->ptr, bytes);
	if (!p)
		return ENOMEM;

	flv->ptr = (uint8_t*)p;
	flv->bytes = bytes;
	return 0;
}

int flv_writer_audio(void* p, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	int n, m;
	struct flv_writer_t* flv;
	flv = (struct flv_writer_t*)p;

	if (flv->bytes < bytes + 11/*TAG*/ + 2/*AudioTagHeader*/ + 2/*AACConfig*/ + 4/*TagSize*/)
	{
		if (0 != flv_write_alloc(flv, bytes + 11/*TAG*/ + 2/*AudioTagHeader*/ + 2/*AACConfig*/ + 4/*TagSize*/))
			return ENOMEM;
	}

	/* ADTS */
	n = mpeg4_aac_adts_load(data, bytes, &flv->aac);
	if(n <= 0)
		return -1; // invalid data

	if (0 == flv->audio)
	{
		m = mpeg4_aac_audio_specific_config_save(&flv->aac, flv->ptr + 13, flv->bytes - 13);
		assert(13 + m + 4 <= (int)flv->bytes);

		flv_write_tag(flv->ptr, FLV_TYPE_AUDIO, m + 2, pts);
		flv->ptr[11] = (10 << 4) /* AAC */ | (3 << 2) /* SoundRate */ | (1 << 1) /* 16-bit samples */ | 1 /* Stereo sound */;
		flv->ptr[12] = flv->audio;
		be_write_uint32(flv->ptr + 13 + m, 13 + m); // TAG size

		if (m + 17 != (int)fwrite(flv->ptr, 1, m + 17, flv->fp))
			return ferror(flv->fp);

		flv->audio = 1;
	}

	dts = 0;
	flv_write_tag(flv->ptr, FLV_TYPE_AUDIO, bytes - n + 2, pts);
	flv->ptr[11] = (10 << 4) /* AAC */ | (3 << 2) /* 44k-SoundRate */ | (1 << 1) /* 16-bit samples */ | 1 /* Stereo sound */;
	flv->ptr[12] = flv->audio;
	memcpy(flv->ptr + 13, (uint8_t*)data + n, bytes - n); // AAC exclude ADTS
	be_write_uint32(flv->ptr + 13 + bytes - n, bytes - n + 13); // TAG size

	if (bytes - n + 17 != fwrite(flv->ptr, 1, bytes - n + 17, flv->fp))
		return ferror(flv->fp);

	return 0;
}

static void flv_h264_handler(void* param, const void* nalu, size_t bytes)
{
	struct mpeg4_avc_t* avc;
	struct flv_writer_t* flv;
	int type = ((unsigned char*)nalu)[0] & 0x1f;

	flv = (struct flv_writer_t*)param;
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
		be_write_uint32(flv->ptr + flv->offset, bytes);
		memcpy(flv->ptr + flv->offset + 4, nalu, bytes);
		flv->offset += bytes + 4;
	}
}

int flv_writer_video(void* p, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	int m, compositionTime;
	struct flv_writer_t* flv;
	flv = (struct flv_writer_t*)p;

	if (flv->bytes < bytes + 1024)
	{
		if (0 != flv_write_alloc(flv, bytes + 1024))
			return ENOMEM;
	}

	if (0 == flv->video)
	{
		flv->offset = 16;
		flv->keyframe = 0;
		flv->avc.nb_sps = 0;
		flv->avc.nb_pps = 0;
		h264_stream(data, bytes, flv_h264_handler, flv);

		if (flv->avc.nb_sps < 1 || flv->avc.sps[0].bytes < 4)
			return 0;

		flv->avc.profile = flv->avc.sps[0].data[1];
		flv->avc.compatibility = flv->avc.sps[0].data[2];
		flv->avc.level = flv->avc.sps[0].data[3];
		flv->avc.nalu = 4;

		m = mpeg4_avc_decoder_configuration_record_save(&flv->avc, flv->ptr + 16, flv->bytes - 16);
		if (m <= 0)
			return -1; // invalid data

		flv_write_tag(flv->ptr, FLV_TYPE_VDIEO, m + 5, dts);
		flv->ptr[11] = ((flv->keyframe?1:2) << 4) /* FrameType */ | 7 /* AVC */;
		flv->ptr[12] = 0; // AVC sequence header
		flv->ptr[13] = 0;
		flv->ptr[14] = 0;
		flv->ptr[15] = 0;
		be_write_uint32(flv->ptr + 16 + m, 16 + m); // TAG size

		if (m + 20 != (int)fwrite(flv->ptr, 1, m + 20, flv->fp))
			return ferror(flv->fp);

		flv->video = 1;
	}

	flv->offset = 16;
	flv->keyframe = 0;
	flv->avc.nb_sps = 0;
	flv->avc.nb_pps = 0;
	h264_stream(data, bytes, flv_h264_handler, flv);

	assert(flv->offset - 16 >= bytes);
	compositionTime = pts - dts;
	flv_write_tag(flv->ptr, FLV_TYPE_VDIEO, flv->offset - 11, dts);
	flv->ptr[11] = ((flv->keyframe ? 1 : 2) << 4) /* FrameType */ | 7 /* AVC */;
	flv->ptr[12] = 1; // AVC NALU
	flv->ptr[13] = (compositionTime >> 16) & 0xFF;
	flv->ptr[14] = (compositionTime >> 8) & 0xFF;
	flv->ptr[15] = compositionTime & 0xFF;
	be_write_uint32(flv->ptr + flv->offset, flv->offset); // TAG size

	if (flv->offset + 4 != fwrite(flv->ptr, 1, flv->offset + 4, flv->fp))
		return ferror(flv->fp);

	return 0;
}
