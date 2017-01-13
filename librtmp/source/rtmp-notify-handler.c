#include "rtmp-internal.h"
#include "rtmp-event.h"
#include "rtmp-util.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// s -> c
int rtmp_notify_stream_begin(uint8_t* data, size_t bytes, uint32_t streamId)
{
	if (bytes < 6) return 0;
	be_write_uint16(data, RTMP_USER_EVENT_STREAM_BEGIN);
	be_write_uint32(data + 2, streamId);
	return 6;
}

// s -> c
int rtmp_notify_stream_eof(uint8_t* data, size_t bytes, uint32_t streamId)
{
	if (bytes < 6) return 0;
	be_write_uint16(data, RTMP_USER_EVENT_STREAM_EOF);
	be_write_uint32(data + 2, streamId);
	return 6;
}

// s -> c
int rtmp_notify_stream_dry(uint8_t* data, size_t bytes, uint32_t streamId)
{
	if (bytes < 6) return 0;
	be_write_uint16(data, RTMP_USER_EVENT_STREAM_DRY);
	be_write_uint32(data + 2, streamId);
	return 6;
}

// c -> s
int rtmp_notify_set_buffer_length(uint8_t* data, size_t bytes, uint32_t streamId, uint32_t ms)
{
	if (bytes < 10) return 0;
	be_write_uint16(data, RTMP_USER_EVENT_SET_BUFFER_LENGTH);
	be_write_uint32(data + 2, streamId);
	be_write_uint32(data + 6, ms);
	return 10;
}

// s -> c
int rtmp_notify_stream_is_record(uint8_t* data, size_t bytes, uint32_t streamId)
{
	if (bytes < 6) return 0;
	be_write_uint16(data, RTMP_USER_EVENT_STREAM_IS_RECORD);
	be_write_uint32(data + 2, streamId);
	return 6;
}

// s -> c
int rtmp_notify_ping(uint8_t* data, size_t bytes, uint32_t timstamp)
{
	if (bytes < 6) return 0;
	be_write_uint16(data, RTMP_USER_EVENT_PING_REQUEST);
	be_write_uint32(data + 2, timstamp);
	return 6;
}

// c -> s
int rtmp_notify_pong(uint8_t* data, size_t bytes, uint32_t timstamp)
{
	if (bytes < 6) return 0;
	be_write_uint16(data, RTMP_USER_EVENT_PING_RESPONSE);
	be_write_uint32(data + 2, timstamp);
	return 6;
}

int rtmp_notify_handler(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* data)
{
	uint16_t event = 0;
	uint32_t streamId = 0;

	if (header->length < 6) return 0;
	be_read_uint16(data, &event);
	be_read_uint32(data + 2, &streamId);

	switch (event)
	{
	case RTMP_USER_EVENT_STREAM_BEGIN:
	case RTMP_USER_EVENT_STREAM_EOF:
	case RTMP_USER_EVENT_STREAM_DRY:
	case RTMP_USER_EVENT_STREAM_IS_RECORD:
		return 6;

	case RTMP_USER_EVENT_SET_BUFFER_LENGTH:
		if (header->length < 10) return 0;
		be_read_uint32(data + 6, &rtmp->buffer_length_ms);
		return 10;

	case RTMP_USER_EVENT_PING_REQUEST:
	case RTMP_USER_EVENT_PING_RESPONSE:
		rtmp->onping(rtmp->param, streamId);
		return 6;

	default:
		printf("unknown user control message event: %u\n", (unsigned int)event);
		return 0;
	}
}
