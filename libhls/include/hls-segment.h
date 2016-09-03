#ifndef _hls_segment_h_
#define _hls_segment_h_

struct hls_segment_t
{
	int64_t pts;		// present timestamp (millisecond)
	int64_t duration;	// segment duration (millisecond)

	uint64_t m3u8seq;	// m3u8 file sequence number (base 0)
	int discontinue; // EXT-X-DISCONTINUITY flag

	char name[128];
};

#endif /* !_hls_segment_h_ */
