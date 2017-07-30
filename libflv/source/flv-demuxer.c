#include "flv-demuxer.h"
#include "flv-proto.h"
#include "mpeg4-aac.h"
#include "mpeg4-avc.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define FLV_VIDEO_KEY_FRAME	1

#define N_FLV_HEADER		9		// DataOffset included
#define N_TAG_HEADER		11		// StreamID included
#define N_TAG_SIZE			4		// previous tag size

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

	uint8_t* ptr;
	uint32_t capacity;
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

	if (flv->ptr)
	{
		assert(flv->capacity > 0);
		free(flv->ptr);
	}

	free(flv);
}

static int flv_demuxer_check_and_alloc(struct flv_demuxer_t* flv, size_t bytes)
{
	if (bytes > flv->capacity)
	{
		void* p = realloc(flv->ptr, bytes);
		if (NULL == p)
			return -1;
		flv->ptr = (uint8_t*)p;
		flv->capacity = bytes;
	}
	return 0;
}

static int flv_demuxer_audio(struct flv_demuxer_t* flv, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
	flv->audio.format = (data[0] & 0xF0) /*>> 4*/;
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
			return flv->handler(flv->param, FLV_AUDIO_ASC, data + 2, bytes - 2, timestamp, timestamp, 0);
		}
		else
		{
			if (0 != flv_demuxer_check_and_alloc(flv, bytes + 7))
				return -ENOMEM;

			// AAC ES stream with ADTS header
			assert(bytes <= 0x1FFF);
			assert(bytes > 2 && 0xFFF0 != (((data[2] << 8) | data[3]) & 0xFFF0)); // don't have ADTS
			mpeg4_aac_adts_save(&flv->aac, (uint16_t)bytes - 2, flv->ptr, 7); // 13-bits
			memmove(flv->ptr + 7, data + 2, bytes - 2);
			return flv->handler(flv->param, FLV_AUDIO_AAC, flv->ptr, bytes - 2 + 7, timestamp, timestamp, 0);
		}
	}
	else if (FLV_AUDIO_MP3 == flv->audio.format || FLV_AUDIO_MP3_8K == flv->audio.format)
	{
		return flv->handler(flv->param, flv->audio.format, data + 1, bytes - 1, timestamp, timestamp, 0);
	}
	else
	{
		// Audio frame data
		return flv->handler(flv->param, flv->audio.format, data + 1, bytes - 1, timestamp, timestamp, 0);
	}
}

static int flv_demuxer_video(struct flv_demuxer_t* flv, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
	size_t n;
	uint8_t packetType; // 0-AVC sequence header, 1-AVC NALU, 2-AVC end of sequence
	uint32_t compositionTime; // 0

	flv->video.frame = (data[0] & 0xF0) >> 4;
	flv->video.codecid = (data[0] & 0x0F);

	if (FLV_VIDEO_H264 == flv->video.codecid)
	{
		packetType = data[1];
		compositionTime = (data[2] << 16) | (data[3] << 8) | data[4];

		if (0 == packetType)
		{
			// AVCDecoderConfigurationRecord
			assert(bytes > 5 + 7);
			mpeg4_avc_decoder_configuration_record_load(data + 5, bytes - 5, &flv->avc);
			return flv->handler(flv->param, FLV_VIDEO_AVCC, data + 5, bytes - 5, timestamp, timestamp, 0);
		}
		else if(1 == packetType)
		{
			assert(flv->avc.nalu > 0); // parse AVCDecoderConfigurationRecord failed
			if (flv->avc.nalu > 0 && bytes > 5) // 5 ==  bytes flv eof
			{
				// H.264
				if (0 != flv_demuxer_check_and_alloc(flv, bytes + 4 * 1024))
					return -ENOMEM;

				assert(flv->avc.nalu <= 4);
				n = mpeg4_mp4toannexb(&flv->avc, data + 5, bytes - 5, flv->ptr, flv->capacity);
				if (n <= 0 || n > flv->capacity)
				{
					assert(0);
					return -ENOMEM;
				}
				return flv->handler(flv->param, FLV_VIDEO_H264, flv->ptr, n, timestamp + compositionTime, timestamp, (FLV_VIDEO_KEY_FRAME == flv->video.frame) ? 1 : 0);
			}
			return -EINVAL;
		}
		else if (2 == packetType)
		{
			return 0; // AVC end of sequence (lower level NALU sequence ender is not required or supported)
		}
		else
		{
			assert(0);
			return -EINVAL;
		}
	}
	else
	{
		// Video frame data
		return flv->handler(flv->param, flv->video.codecid, data + 1, bytes - 1, timestamp, timestamp, (FLV_VIDEO_KEY_FRAME==flv->video.frame) ? 1 : 0);
	}
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
	case FLV_TYPE_AUDIO:
		return flv_demuxer_audio(flv, data, bytes, timestamp);

	case FLV_TYPE_VIDEO:
		return flv_demuxer_video(flv, data, bytes, timestamp);

	case FLV_TYPE_SCRIPT:
		//return flv_demuxer_script(flv, data, bytes);
		return 0;

	default:
		assert(0);
		return -1;
	}
}
