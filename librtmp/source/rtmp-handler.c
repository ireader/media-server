#include "rtmp-internal.h"
#include "rtmp-msgtypeid.h"
#include <stdio.h>
#include <assert.h>

static void rtmp_audio(struct rtmp_t* rtmp, struct rtmp_chunk_header_t* header, const uint8_t* payload)
{
	rtmp->u.client.onaudio(rtmp->param, payload, header->length, header->timestamp);
}

static void rtmp_video(struct rtmp_t* rtmp, struct rtmp_chunk_header_t* header, const uint8_t* payload)
{
	rtmp->u.client.onvideo(rtmp->param, payload, header->length, header->timestamp);
}

int rtmp_handler(struct rtmp_t* rtmp, struct rtmp_chunk_header_t* header, const uint8_t* payload)
{
	switch (header->type)
	{
	case RTMP_TYPE_INVOKE:
		return rtmp_invoke_handler(rtmp, header, payload);

	case RTMP_TYPE_VIDEO:
		rtmp_video(rtmp, header, payload);
		break;

	case RTMP_TYPE_AUDIO:
		rtmp_audio(rtmp, header, payload);
		break;

	case RTMP_TYPE_EVENT:
		// User Control Message Events
		return rtmp_event_handler(rtmp, header, payload);

		// Protocol Control Messages
	case RTMP_TYPE_SET_CHUNK_SIZE:
	case RTMP_TYPE_ABORT:
	case RTMP_TYPE_ACKNOWLEDGEMENT:
	case RTMP_TYPE_WINDOW_ACKNOWLEDGEMENT_SIZE:
	case RTMP_TYPE_SET_PEER_BANDWIDTH:
		return rtmp_control_handler(rtmp, header, payload);

	case RTMP_TYPE_NOTIFY:
	case RTMP_TYPE_FLEX_STREAM:
		// play -> RtmpSampleAccess
		// finish -> onPlayStatus("NetStream.Play.Complete")
		break;

	case RTMP_TYPE_SHARED_OBJECT:
	case RTMP_TYPE_FLEX_OBJECT:
		break;

	case RTMP_TYPE_METADATA:
		break;

	default:
		// handshake
		assert(0);
		printf("%s: unknown rtmp header type: %d\n", __FUNCTION__, (int)header->type);
		break;
	}

	return 0;
}
