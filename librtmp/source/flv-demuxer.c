#include "flv-demuxer.h"
#include "mpeg4-aac.h"
#include "mpeg4-avc.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

// FLV Tag Type
#define FLV_AUDIO			8
#define FLV_VIDEO			9
#define FLV_SCRIPT			18

// FLV Audio Type
#define FLV_AUDIO_ADPCM		1
#define FLV_AUDIO_MP3		2
#define FLV_AUDIO_G711		7
#define FLV_AUDIO_AAC		10
#define FLV_AUDIO_MP3_8k	14

// FLV Video Type
#define FLV_VIDEO_H263		2
#define FLV_VIDEO_VP6		4
#define FLV_VIDEO_AVC		7

#define N_FLV_HEADER		9		// DataOffset included
#define N_TAG_HEADER		11		// StreamID included
#define N_TAG_SIZE			4		// previous tag size

#define N_SPSPPS			4096

#define H264_NAL_IDR		5 // Coded slice of an IDR picture
#define H264_NAL_SPS		7 // Sequence parameter set
#define H264_NAL_PPS		8 // Picture parameter set

struct flv_audio_tag_t
{
	uint8_t format; // 1-ADPCM, 2-MP3, 10-AAC, 14-MP3 8kHz
	uint8_t bitrate; // 0-5.5kHz, 1-11kHz, 2-22kHz,3-44kHz
	uint8_t bitsPerSample; // 0-8bits, 1-16bits
	uint8_t channel; // 0-Mono sound, ,1-Stereo sound
};

struct flv_video_tag_t
{
	uint8_t frame; // 1-key frame, 2-inter frame, 3-disposable inter frame(H.263 only), 4-generated key frame, 5-video info/command frame
	uint8_t codecid; // 2-Sorenson H.263, 3-Screen video 4-On2 VP6, 7-AVC
};

struct flv_demuxer_t
{
	struct flv_audio_tag_t audio;
	struct flv_video_tag_t video;
	struct mpeg4_aac_t aac;
	struct mpeg4_avc_t avc;

	flv_demuxer_handler handler;
	void* param;

	uint8_t ps[N_SPSPPS]; // SPS/PPS
	uint32_t pslen;

	uint8_t* data;
	uint32_t bytes;
};

void* flv_demuxer_create(flv_demuxer_handler handler, void* param)
{
	struct flv_demuxer_t* flv;
	flv = (struct flv_demuxer_t*)malloc(sizeof(struct flv_demuxer_t));
	if (NULL == flv)
		return NULL;

	memset(flv, 0, sizeof(struct flv_demuxer_t));
	flv->handler = handler;
	flv->param = param;
	return flv;
}

void flv_demuxer_destroy(void* demuxer)
{
	struct flv_demuxer_t* flv;
	flv = (struct flv_demuxer_t*)demuxer;

	if (flv->data)
	{
		assert(flv->bytes > 0);
		free(flv->data);
	}

	free(flv);
}

static int flv_demuxer_check_and_alloc(struct flv_demuxer_t* flv, size_t bytes)
{
	if (bytes > flv->bytes)
	{
		void* p = realloc(flv->data, bytes + 2 * 1024);
		if (NULL == p)
			return -1;
		flv->data = (uint8_t*)p;
		flv->bytes = bytes + 2 * 1024;
	}
	return 0;
}

static int flv_demuxer_audio(struct flv_demuxer_t* flv, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
	flv->audio.format = (data[0] & 0xF0) >> 4;
	flv->audio.bitrate = (data[0] & 0x0C) >> 2;
	flv->audio.bitsPerSample = (data[0] & 0x02) >> 1;
	flv->audio.channel = data[0] & 0x01;

	if (FLV_AUDIO_AAC == flv->audio.format)
	{
		// Adobe Flash Video File Format Specification Version 10.1 >> E.4.2.1 AUDIODATA (p77)
		// If the SoundFormat indicates AAC, the SoundType should be 1 (stereo) and the SoundRate should be 3 (44 kHz).
		// However, this does not mean that AAC audio in FLV is always stereo, 44 kHz data.Instead, the Flash Player ignores
		// these values and extracts the channel and sample rate data is encoded in the AAC bit stream.
		//assert(3 == flv->audio.bitrate && 1 == flv->audio.channel);

		if (0 == data[1])
		{
			mpeg4_aac_audio_specific_config_load(data + 2, bytes - 2, &flv->aac);
			flv->handler(flv->param, FLV_AAC_HEADER, data + 2, bytes - 2, timestamp, timestamp);
		}
		else
		{
			if (0 != flv_demuxer_check_and_alloc(flv, bytes + 7))
				return ENOMEM;

			// AAC ES stream with ADTS header
			assert(bytes <= 0x1FFF);
			assert(bytes > 2 && 0xFFF0 != (((data[2] << 8) | data[3]) & 0xFFF0)); // don't have ADTS
			mpeg4_aac_adts_save(&flv->aac, (uint16_t)bytes - 2, flv->data, 7); // 13-bits
			memmove(flv->data + 7, data + 2, bytes - 2);
			flv->handler(flv->param, FLV_AAC, flv->data, bytes - 2 + 7, timestamp, timestamp);
		}
	}
	else if (FLV_AUDIO_MP3 == flv->audio.format || FLV_AUDIO_MP3_8k == flv->audio.format)
	{
		flv->handler(flv->param, flv->audio.format, data + 1, bytes - 1, timestamp, timestamp);
	}
	else
	{
		// Audio frame data
		assert(0);
		flv->handler(flv->param, flv->audio.format, data + 1, bytes - 1, timestamp, timestamp);
	}

	return 0;
}

static int flv_demuxer_video(struct flv_demuxer_t* flv, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
	uint8_t packetType; // 0-AVC sequence header, 1-AVC NALU, 2-AVC end of sequence
	uint32_t compositionTime; // 0

	flv->video.frame = (data[0] & 0xF0) >> 4;
	flv->video.codecid = (data[0] & 0x0F);

	if (FLV_VIDEO_AVC == flv->video.codecid)
	{
		packetType = data[1];
		compositionTime = (data[2] << 16) | (data[3] << 8) | data[4];

		if (0 == packetType)
		{
			// AVCDecoderConfigurationRecord
			assert(bytes > 5 + 7);
			if (bytes > 5 + 7)
			{
				//uint8_t version = data[5];
				//uint8_t profile = data[6];
				//uint8_t flags = data[7];
				//uint8_t level = data[8];
				//flv->video.nal = (data[9] & 0x03) + 1;
				assert(sizeof(flv->ps) > bytes + 128);
				if (sizeof(flv->ps) > bytes + 128)
				{
					mpeg4_avc_decoder_configuration_record_load(data + 5, bytes - 5, &flv->avc);
					flv->pslen = mpeg4_avc_to_nalu(&flv->avc, flv->ps, sizeof(flv->ps));
					assert(flv->pslen < sizeof(flv->ps));
				}
				flv->handler(flv->param, FLV_AVC_HEADER, data + 5, bytes - 5, timestamp, timestamp);
			}
		}
		else
		{
			// H.264
			uint32_t k = 0;
			uint8_t sps_pps_flag = 0;
			const uint8_t* p = data + 5;
			const uint8_t* end = data + bytes;

			if (0 != flv_demuxer_check_and_alloc(flv, bytes + 4 + flv->pslen))
				return ENOMEM;

			assert(flv->avc.nalu <= 4);
			while (p + flv->avc.nalu < end)
			{
				int i;
				uint8_t nal;
				size_t n = 0;
				for (i = 0; i < flv->avc.nalu; i++)
					n = (n << 8) + p[i];

				// fix 0x00 00 00 01 => flv nalu size
				if (1 == n)
					n = end - p - flv->avc.nalu;

				if (p + flv->avc.nalu + n > end)
					break; // invalid nalu size

				// insert SPS/PPS before IDR frame
				nal = p[flv->avc.nalu] & 0x1f;
				if (H264_NAL_SPS == nal || H264_NAL_PPS == nal)
				{
					//flv->data[k++] = 0; // SPS/PPS add zero_byte(ITU H.264 B.1.2 Byte stream NAL unit semantics)
					sps_pps_flag = 1;
				}
				else if (H264_NAL_IDR == nal && 0 == sps_pps_flag)
				{
					sps_pps_flag = 1; // don't insert more than one-times
					memcpy(flv->data + k, flv->ps, flv->pslen); //
					k += flv->pslen;
				}

				// nalu start code
				flv->data[k] = flv->data[k + 1] = flv->data[k + 2] = 0x00;
				flv->data[k + 3] = 0x01;
				memcpy(flv->data + k + 4, p + flv->avc.nalu, n);

				k += n + 4;
				p += flv->avc.nalu + n;
			}

			flv->handler(flv->param, FLV_AVC, flv->data, k, timestamp + compositionTime, timestamp);
		}
	}
	else
	{
		// Video frame data
		assert(0);
		flv->handler(flv->param, flv->video.codecid << 4, data + 1, bytes - 1, timestamp, timestamp);
	}

	return 0;
}

// http://www.cnblogs.com/musicfans/archive/2012/11/07/2819291.html
// metadata keyframes/filepositions
//static int flv_demuxer_script(struct flv_demuxer_t* flv, const uint8_t* data, size_t bytes)
//{
//	// FLV I-index
//	return 0;
//}

int flv_demuxer_input(void* p, int type, const void* data, size_t bytes, uint32_t timestamp)
{
	struct flv_demuxer_t* flv;
	flv = (struct flv_demuxer_t*)p;

	switch (type)
	{
	case FLV_AUDIO:
		return flv_demuxer_audio(flv, data, bytes, timestamp);

	case FLV_VIDEO:
		return flv_demuxer_video(flv, data, bytes, timestamp);

	case FLV_SCRIPT:
		//return flv_demuxer_script(flv, data, bytes);
		return 0;

	default:
		assert(0);
		return -1;
	}
}
