#include "rtmp-internal.h"
#include "rtmp-msgtypeid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define RTMP_MESSAGE_HEADER_LENGTH	11 // RTMP 6.1.1

static int rtmp_audio(struct rtmp_t* rtmp, const uint8_t* payload, uint32_t bytes, uint32_t timestamp)
{
	return rtmp->onaudio(rtmp->param, payload, bytes, timestamp);
}

static int rtmp_video(struct rtmp_t* rtmp, const uint8_t* payload, uint32_t bytes, uint32_t timestamp)
{
	return rtmp->onvideo(rtmp->param, payload, bytes, timestamp);
}

static int rtmp_script(struct rtmp_t* rtmp, const uint8_t* payload, uint32_t bytes, uint32_t timestamp)
{
	// filter @setDataFrame
	const static uint8_t s_setFrameData[] = { 0x02, 0x00, 0x0d, 0x40, 0x73, 0x65, 0x74, 0x44, 0x61, 0x74, 0x61, 0x46, 0x72, 0x61, 0x6d, 0x65 };
	if (bytes > sizeof(s_setFrameData) && 0 == memcmp(s_setFrameData, payload, sizeof(s_setFrameData)))
		return rtmp->onscript(rtmp->param, payload + sizeof(s_setFrameData), bytes - sizeof(s_setFrameData), timestamp);

	return rtmp->onscript(rtmp->param, payload, bytes, timestamp);
}

static int rtmp_aggregate(struct rtmp_t* rtmp, struct rtmp_chunk_header_t* header, const uint8_t* payload)
{
	int r;
	uint32_t i;
	uint32_t n;
	uint32_t t, t0, timestamp;

	timestamp = header->timestamp;
	for (t0 = i = r = 0; i + RTMP_MESSAGE_HEADER_LENGTH + 4 /*previous tag size*/ < header->length && 0 == r; i+= n + RTMP_MESSAGE_HEADER_LENGTH + 4)
	{
		n = ((uint32_t)payload[i + 1] << 16) | ((uint32_t)payload[i + 2] << 8) | payload[i + 3];
		t = ((uint32_t)payload[i + 4] << 16) | ((uint32_t)payload[i + 5] << 8) | payload[i + 6] | ((uint32_t)payload[i + 7] << 24);
		if (i + n + RTMP_MESSAGE_HEADER_LENGTH + 4 > header->length)
		{
			assert(0);
			return 0; // ignore
		}

		if (0 != i)
			timestamp += t - t0;
		t0 = t;

		switch (payload[i])
		{
		case RTMP_TYPE_AUDIO:
			r = rtmp_audio(rtmp, payload + i + RTMP_MESSAGE_HEADER_LENGTH, n, timestamp);
			break;

		case RTMP_TYPE_VIDEO:
			r = rtmp_video(rtmp, payload + i + RTMP_MESSAGE_HEADER_LENGTH, n, timestamp);
			break;

		case RTMP_TYPE_DATA:
			r = rtmp_script(rtmp, payload + i + RTMP_MESSAGE_HEADER_LENGTH, n, timestamp);
			break;

		default:
			assert(0);
			r = 0;
			break;
		}
	}

	assert(r || i == header->length);
	return r;
}

int rtmp_handler(struct rtmp_t* rtmp, struct rtmp_chunk_header_t* header, const uint8_t* payload)
{
	switch (header->type)
	{
	case RTMP_TYPE_FLEX_MESSAGE:
		// filter AMF3 0x00
		payload += (header->length > 0) ? 1 : 0; // fix header->length = 0
		header->length -= (header->length > 0) ? 1 : 0;
		return rtmp_invoke_handler(rtmp, header, payload);

	case RTMP_TYPE_INVOKE:
		return rtmp_invoke_handler(rtmp, header, payload);

	case RTMP_TYPE_VIDEO:
		return rtmp_video(rtmp, payload, header->length, header->timestamp);

	case RTMP_TYPE_AUDIO:
		return rtmp_audio(rtmp, payload, header->length, header->timestamp);

	case RTMP_TYPE_EVENT:
		// User Control Message Events
		return 0 == rtmp_event_handler(rtmp, header, payload) ? -1 : 0;

		// Protocol Control Messages
	case RTMP_TYPE_SET_CHUNK_SIZE:
	case RTMP_TYPE_ABORT:
	case RTMP_TYPE_ACKNOWLEDGEMENT:
	case RTMP_TYPE_WINDOW_ACKNOWLEDGEMENT_SIZE:
	case RTMP_TYPE_SET_PEER_BANDWIDTH:
		return 0 == rtmp_control_handler(rtmp, header, payload) ? -1 : 0;

	case RTMP_TYPE_DATA:
	case RTMP_TYPE_FLEX_STREAM:
		// play -> RtmpSampleAccess
		// finish -> onPlayStatus("NetStream.Play.Complete")
		return rtmp_script(rtmp, payload, header->length, header->timestamp);

	case RTMP_TYPE_SHARED_OBJECT:
	case RTMP_TYPE_FLEX_OBJECT:
		break;

	case RTMP_TYPE_METADATA:
		return rtmp_aggregate(rtmp, header, payload);

	default:
		assert(0);
		printf("%s: unknown rtmp header type: %d\n", __FUNCTION__, (int)header->type);
		break;
	}

	return 0;
}
