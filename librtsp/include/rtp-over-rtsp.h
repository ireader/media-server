#ifndef _rtp_over_rtsp_h_
#define _rtp_over_rtsp_h_

#include <stdint.h>

struct rtp_over_rtsp_t
{
	int state; // 0-all done, other-need more interleaved data
	uint8_t channel;
	uint16_t length;
	uint16_t bytes;
	uint16_t capacity;
	uint8_t* data;

	void (*onrtp)(void* param, uint8_t channel, const void* data, uint16_t bytes);
	void* param;
};

const uint8_t* rtp_over_rtsp(struct rtp_over_rtsp_t *rtp, const uint8_t* data, const uint8_t* end);

#endif /* !_rtp_over_rtsp_h_ */
