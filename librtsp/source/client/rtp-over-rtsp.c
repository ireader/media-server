#include "rtsp-client-internal.h"

#define VMIN(x, y) ((x) < (y) ? (x) : (y))

enum rtp_over_tcp_state_t
{
	rtp_start = 0,
	rtp_channel,
	rtp_length_1,
	rtp_length_2,
	rtp_data,
};

static int rtp_alloc(struct rtsp_client_t *rtsp)
{
	void* p;
	if (rtsp->rtp.capacity < rtsp->rtp.length)
	{
		p = realloc(rtsp->rtp.data, rtsp->rtp.length);
		if (!p)
			return -1;
		rtsp->rtp.data = (uint8_t*)p;
		rtsp->rtp.capacity = rtsp->rtp.length;
	}
	return 0;
}

// 10.12 Embedded (Interleaved) Binary Data
// Stream data such as RTP packets is encapsulated by an ASCII dollar sign(24 hexadecimal), 
// followed by a one-byte channel identifier,
// followed by the length of the encapsulated binary data as a binary two-byte integer in network byte order.
const uint8_t* rtp_over_rtsp(struct rtsp_client_t *rtsp, const uint8_t* data, const uint8_t* end)
{
	int n;

	for (n = 0; data < end; data++)
	{
		switch (rtsp->rtp.state)
		{
		case rtp_start:
			assert(*data == '$');
			rtsp->rtp.bytes = 0;
			rtsp->rtp.state = rtp_channel;
			break;

		case rtp_channel:
			// The channel identifier is defined in the Transport header with 
			// the interleaved parameter(Section 12.39).
			rtsp->rtp.channel = *data;
			rtsp->rtp.state = rtp_length_1;
			break;

		case rtp_length_1:
			rtsp->rtp.length = *data;
			rtsp->rtp.state = rtp_length_2;
			break;

		case rtp_length_2:
			rtsp->rtp.length = (rtsp->rtp.length << 8) | *data;
			rtsp->rtp.state = rtp_data;
			break;

		case rtp_data:
			if (0 == rtsp->rtp.bytes && 0 != rtp_alloc(rtsp))
				return end;
			n = end - data;
			n = VMIN(rtsp->rtp.length - rtsp->rtp.bytes, n);
			memcpy(rtsp->rtp.data + rtsp->rtp.bytes, data, n);
			rtsp->rtp.bytes += (uint16_t)n;
			data += n;

			if (rtsp->rtp.bytes == rtsp->rtp.length)
			{
				rtsp->rtp.state = rtp_start;
				rtsp->handler.onrtp(rtsp->param, rtsp->rtp.channel, rtsp->rtp.data, rtsp->rtp.length);
				return data;
			}
			break;

		default:
			assert(0);
			return end;
		}
	}

	return data;
}
