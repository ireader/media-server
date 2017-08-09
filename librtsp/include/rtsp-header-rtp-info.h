#ifndef _rtsp_header_rtp_info_h_
#define _rtsp_header_rtp_info_h_

#include <stdint.h>

struct rtsp_header_rtp_info_t
{
	char url[256];
	uint64_t seq;
	uint64_t rtptime;
};

/// parse RTSP RTP-Info header
/// @return 0-ok, other-error
/// usage 1:
/// struct rtsp_header_rtp_info_t rtpinfo;
/// const char* header = "RTP-Info: url=rtsp://foo.com/bar.avi/streamid=0;seq=45102";
/// r = rtsp_header_rtp_info("url=rtsp://foo.com/bar.avi/streamid=0;seq=45102", &rtpinfo);
/// check(r)
/// 
/// usage 2:
/// const char* header = "RTP-Info: url=rtsp://foo.com/bar.avi/streamid=0;seq=45102,url=rtsp://foo.com/bar.avi/streamid=1;seq=30211";
/// split(header, ',');
/// r1 = rtsp_header_rtp_info("url=rtsp://foo.com/bar.avi/streamid=0;seq=45102", &rtpinfo);
/// r2 = rtsp_header_rtp_info("url=rtsp://foo.com/bar.avi/streamid=1;seq=30211", &rtpinfo);
/// check(r1, r2)
int rtsp_header_rtp_info(const char* field, struct rtsp_header_rtp_info_t* rtpinfo);

#endif /* !_rtsp_header_rtp_info_h_ */
