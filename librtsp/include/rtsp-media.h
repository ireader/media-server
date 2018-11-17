#ifndef _rtsp_media_h_
#define _rtsp_media_h_

#include "rtsp-header-range.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct rtsp_media_t
{
	char uri[256]; // rtsp setup url
	char session_uri[256]; // rtsp session url(aggregate control url), empty if non-aggregate control
	
	//unsigned int cseq; // rtsp sequence, unused if aggregate control available
	int64_t start, stop; // sdp t: NTP time since 1900
	char network[16], addrtype[16], address[64]; // sdp c: connection
	struct rtsp_header_range_t range;

	int avformat_count;
	struct avformat_t
	{
		int fmt; // RTP payload type
		int rate; // RTP payload frequency
		int channel; // RTP payload channel
		char encoding[64]; // RTP payload encoding
		char fmtp[4 * 1024]; // RTP fmtp value
	} avformats[3];
};

/// @return <0-error, >count-try again, other-ok
int rtsp_media_sdp(const char* sdp, struct rtsp_media_t* medias, int count);

/// update session and media url
/// @param[in] base The RTSP Content-Base field
/// @param[in] location The RTSP Content-Location field
/// @param[in] request The RTSP request URL
/// @param[out] uri media control uri
/// return 0-ok, other-error
int rtsp_media_set_url(struct rtsp_media_t* media, const char* base, const char* location, const char* request);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtsp_media_h_ */
