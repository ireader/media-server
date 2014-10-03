#ifndef _sdp_a_rtpmap_h_
#define _sdp_a_rtpmap_h_

/// parse SDP a=rtpmap:
/// a=rtpmap:98 L16/11025/2
/// sdp_a_rtpmap("98 L16/11025/2", 98, "L16", 11025, "2");
/// @param[in] rtpmap value
/// @param[out] payload RTP payload type
/// @param[out] encoding audio/video encoding [optional, null acceptable]
/// @param[out] rate bit rate [optional, null acceptable]
/// @param[out] parameters payload parameters [optional, null acceptable]
/// @return 0-ok, other-error
int sdp_a_rtpmap(const char* rtpmap, int *payload, char *encoding, int *rate, char *parameters);

#endif /* !_sdp_a_rtpmap_h_ */
