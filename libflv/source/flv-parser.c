#include "flv-parser.h"
#include "flv-header.h"
#include "flv-proto.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define FLV_VIDEO_CODEC_NAME(codecid) (FLV_VIDEO_H264==(codecid) ? FLV_VIDEO_AVCC : (FLV_VIDEO_H265==(codecid) ? FLV_VIDEO_HVCC : FLV_VIDEO_AV1C))

static int flv_parser_audio(const uint8_t* data, int bytes, uint32_t timestamp, flv_parser_handler handler, void* param)
{
	int n;
	struct flv_audio_tag_header_t audio;
	n = flv_audio_tag_header_read(&audio, data, bytes);
	if (n < 0)
		return n;
	if (FLV_AUDIO_AAC == audio.codecid && FLV_SEQUENCE_HEADER == audio.avpacket)
		return handler(param, FLV_AUDIO_ASC, data + n, bytes - n, timestamp, timestamp, 0);
	else
		return handler(param, audio.codecid, data + n, bytes - n, timestamp, timestamp, 0);
}

static int flv_parser_video(const uint8_t* data, int bytes, uint32_t timestamp, flv_parser_handler handler, void* param)
{
	int n;
	struct flv_video_tag_header_t video;
	n = flv_video_tag_header_read(&video, data, bytes);
	if (n < 0)
		return n;

	if (FLV_VIDEO_H264 == video.codecid || FLV_VIDEO_H265 == video.codecid || FLV_VIDEO_AV1 == video.codecid)
	{
		if (FLV_SEQUENCE_HEADER == video.avpacket)
		{
			return handler(param, FLV_VIDEO_CODEC_NAME(video.codecid), data + n, bytes - n, timestamp, timestamp, 0);
		}
		else if (FLV_AVPACKET == video.avpacket)
		{
			return handler(param, video.codecid, data + n, bytes - n, timestamp + video.cts, timestamp, (FLV_VIDEO_KEY_FRAME == video.keyframe) ? 1 : 0);
		}
		else if (FLV_END_OF_SEQUENCE == video.avpacket)
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
		return handler(param, video.codecid, data + n, bytes - n, timestamp, timestamp, (FLV_VIDEO_KEY_FRAME == video.keyframe) ? 1 : 0);
	}
}

// http://www.cnblogs.com/musicfans/archive/2012/11/07/2819291.html
// metadata keyframes/filepositions
static int flv_parser_script(const uint8_t* data, int bytes, uint32_t timestamp, flv_parser_handler handler, void* param)
{
	int n;
	n = flv_data_tag_header_read(data, bytes);
	if (n < 0)
		return n;
	return handler(param, 0, data + n, bytes - n, timestamp, timestamp, 0);
}

int flv_parser_input(int type, const void* data, size_t bytes, uint32_t timestamp, flv_parser_handler handler, void* param)
{
	if (bytes < 1) return -EINVAL;

	switch (type)
	{
	case FLV_TYPE_AUDIO:
		return flv_parser_audio(data, (int)bytes, timestamp, handler, param);

	case FLV_TYPE_VIDEO:
		return flv_parser_video(data, (int)bytes, timestamp, handler, param);

	case FLV_TYPE_SCRIPT:
		return flv_parser_script(data, (int)bytes, timestamp, handler, param);

	default:
		assert(0);
		return -1;
	}
}
