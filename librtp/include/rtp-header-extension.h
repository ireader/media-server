#ifndef _rtp_header_extension_h_
#define _rtp_header_extension_h_

#include <vector>
#include <string>
#include "rtp-ext.h"

template <typename Extension, typename... Values>
bool GetRtpExtensionWithMap(const void* data, const struct rtp_ext_data_t view[256], const uint8_t exts[256], Values... values) {
	uint8_t id = exts[Extension::Id];
	assert(0 == view[id].id || id == view[id].id);
	return 0 != id && id == view[id].id && Extension::Parse((const uint8_t*)data + view[id].off, (uint16_t)view[id].len, values...);
}

template <typename Extension, typename... Values>
bool GetRtpExtension(const void* data, const struct rtp_ext_data_t view[256], Values... values) {
	uint8_t id = view[Extension::Id].id;
	assert(0 == id || id == Extension::Id);
	return 0 != id && Extension::Parse((const uint8_t*)data + view[id].off, (uint16_t)view[id].len, values...);
}

template <typename Extension, typename... Values>
int SetRtpExtension(void* data, int bytes, const struct rtp_ext_data_t view[256], Values... values) {
	if (bytes < Extension::Size)
		return -1;
	return Extension::Write((uint8_t*)data, bytes, view[Extension::Id].id, values...);
}

class RtpExtensionSsrcAudioLevel
{
public:
	static const uint8_t Id = RTP_HDREXT_SSRC_AUDIO_LEVEL_ID;
	static const uint16_t Size = 1;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, uint8_t* activity, uint8_t* level) {
		return 0 == rtp_ext_ssrc_audio_level_parse(data, bytes, activity, level);
	}
	static int Write(uint8_t* data, uint16_t bytes, uint8_t activity, uint8_t level) {
		return rtp_ext_ssrc_audio_level_write(data, bytes, activity, level);
	}
};

class RtpExtensionCsrcAudioLevel
{
public:
	static const uint8_t Id = RTP_HDREXT_CSRC_AUDIO_LEVEL_ID;
	static const uint16_t Size = 15;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, uint8_t* levels, int num) {
		return 0 == rtp_ext_csrc_audio_level_parse(data, bytes, levels, num);
	}
	static int Write(uint8_t* data, uint16_t bytes, const uint8_t* levels, int num) {
		return rtp_ext_csrc_audio_level_write(data, bytes, levels, num);
	}
};

class RtpExtensionFrameMarking
{
public:
	static const uint8_t Id = RTP_HDREXT_FRAME_MARKING_ID;
	static const uint16_t Size = 3;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, rtp_ext_frame_marking_t* ext) {
		return 0 == rtp_ext_frame_marking_parse(data, bytes, ext);
	}
	static int Write(uint8_t* data, uint16_t bytes, const rtp_ext_frame_marking_t* ext) {
		return rtp_ext_frame_marking_write(data, bytes, ext);
	}
};

class RtpExtensionString
{
public:
	static bool Parse(const uint8_t* data, uint16_t bytes, std::string& rid) {
		rid.reserve(bytes);
		return 0 == rtp_ext_string_parse(data, bytes, (char*)rid.data(), rid.capacity());
	}
	static int Write(uint8_t* data, uint16_t bytes, const char* v, int n) {
		return rtp_ext_string_write(data, bytes, v, n);
	}
};

class RtpExtensionSdesMid : public RtpExtensionString
{
public:
	static const uint8_t Id = RTP_HDREXT_SDES_MID_ID;
	static const uint16_t Size = 255;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
};

class RtpExtensionSdesRtpStreamId: public RtpExtensionString
{
public:
	static const uint8_t Id = RTP_HDREXT_SDES_RTP_STREAM_ID;
	static const uint16_t Size = 255;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
};

class RtpExtensionSdesRepairedRtpStreamId : public RtpExtensionString
{
public:
	static const uint8_t Id = RTP_HDREXT_SDES_REPAIRED_RTP_STREAM_ID;
	static const uint16_t Size = 255;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
};

class RtpExtensionTransmissionOffset
{
public:
	static const uint8_t Id = RTP_HDREXT_TOFFSET_ID;
	static const uint16_t Size = 3;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, uint32_t* timestamp) {
		return 0 == rtp_ext_toffset_parse(data, bytes, timestamp);
	}
	static int Write(uint8_t* data, uint16_t bytes, uint32_t timestamp) {
		return rtp_ext_toffset_write(data, bytes, timestamp);
	}
};

class RtpExtensionVideoOrientation
{
public:
	static const uint8_t Id = RTP_HDREXT_VIDEO_ORIENTATION_ID;
	static const uint16_t Size = 1;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, rtp_ext_video_orientation_t* ext) {
		return 0 == rtp_ext_video_orientation_parse(data, bytes, ext);
	}
	static int Write(uint8_t* data, uint16_t bytes, const rtp_ext_video_orientation_t* ext) {
		return rtp_ext_video_orientation_write(data, bytes, ext);
	}
};

class RtpExtensionAbsoluteSendTime
{
public:
	static const uint8_t Id = RTP_HDREXT_ABSOLUTE_SEND_TIME_ID;
	static const uint16_t Size = 3;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, uint64_t* ms) {
		return 0 == rtp_ext_abs_send_time_parse(data, bytes, ms);
	}
	static int Write(uint8_t* data, uint16_t bytes, uint64_t ms) {
		return rtp_ext_abs_send_time_write(data, bytes, ms);
	}
};

class RtpExtensionAbsoluteCaptureTime
{
public:
	static const uint8_t Id = RTP_HDREXT_ABSOLUTE_CAPTURE_TIME_ID;
	static const uint16_t Size = 16;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, rtp_ext_absolute_capture_time_t* ext) {
		return 0 == rtp_ext_absolute_capture_time_parse(data, bytes, ext);
	}
	static int Write(uint8_t* data, uint16_t bytes, const rtp_ext_absolute_capture_time_t* ext) {
		return rtp_ext_absolute_capture_time_write(data, bytes, ext);
	}
};

class RtpExtensionTransportSequenceNumber
{
public:
	static const uint8_t Id = RTP_HDREXT_TRANSPORT_WIDE_CC_ID;
	static const uint16_t Size = 4;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, rtp_ext_transport_wide_cc_t* ext) {
		return 0 == rtp_ext_transport_wide_cc_parse(data, bytes, ext);
	}
	static int Write(uint8_t* data, uint16_t bytes, const rtp_ext_transport_wide_cc_t* ext) {
		return rtp_ext_transport_wide_cc_write(data, bytes, ext);
	}
};

class RtpExtensionVideoTiming
{
public:
	static const uint8_t Id = RTP_HDREXT_VIDEO_TIMING_ID;
	static const uint16_t Size = 13;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, rtp_ext_video_timing_t* ext) {
		return 0 == rtp_ext_video_timing_parse(data, bytes, ext);
	}
	static int Write(uint8_t* data, uint16_t bytes, const rtp_ext_video_timing_t* ext) {
		return rtp_ext_video_timing_write(data, bytes, ext);
	}
};

class RtpExtensionPlayoutDelay
{
public:
	static const uint8_t Id = RTP_HDREXT_PLAYOUT_DELAY_ID;
	static const uint16_t Size = 3;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, rtp_ext_playout_delay_t* ext) {
		return 0 == rtp_ext_playout_delay_parse(data, bytes, ext);
	}
	static int Write(uint8_t* data, uint16_t bytes, const rtp_ext_playout_delay_t* ext) {
		return rtp_ext_playout_delay_write(data, bytes, ext);
	}
};

class RtpExtensionColorSpace
{
public:
	static const uint8_t Id = RTP_HDREXT_COLOR_SPACE_ID;
	static const uint16_t Size = 28;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, rtp_ext_color_space_t* ext) {
		return 0 == rtp_ext_color_space_parse(data, bytes, ext);
	}
	static int Write(uint8_t* data, uint16_t bytes, const rtp_ext_color_space_t* ext) {
		return rtp_ext_color_space_write(data, bytes, ext);
	}
};

class RtpExtensionVideoContentType
{
public:
	static const uint8_t Id = RTP_HDREXT_VIDEO_CONTENT_TYPE_ID;
	static const uint16_t Size = 1;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, uint8_t* ext) {
		return 0 == rtp_ext_video_content_type_parse(data, bytes, ext);
	}
	static int Write(uint8_t* data, uint16_t bytes, uint8_t ext) {
		return rtp_ext_video_content_type_write(data, bytes, ext);
	}
};

class RtpExtensionInbandComfortNoise
{
public:
	static const uint8_t Id = RTP_HDREXT_INBAND_CN_ID;
	static const uint16_t Size = 1;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, uint8_t* level) {
		return 0 == rtp_ext_inband_cn_parse(data, bytes, level);
	}
	static int Write(uint8_t* data, uint16_t bytes, uint8_t level) {
		return rtp_ext_inband_cn_write(data, bytes, level);
	}
};

class RtpExtensionVideoFrameTracking
{
public:
	static const uint8_t Id = RTP_HDREXT_VIDEO_FRAME_TRACKING_ID;
	static const uint16_t Size = 2;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, uint16_t* ext) {
		return 0 == rtp_ext_video_frame_tracking_id_parse(data, bytes, ext);
	}
	static int Write(uint8_t* data, uint16_t bytes, uint16_t ext) {
		return rtp_ext_video_frame_tracking_id_write(data, bytes, ext);
	}
};

class RtpExtensionVideoLayersAllocation
{
public:
	static const uint8_t Id = RTP_HDREXT_VIDEO_LAYERS_ALLOCATION_ID;
	static const uint16_t Size = 2;
	static const char* Uri() {
		return rtp_ext_list()[Id].uri;
	}
	static bool Parse(const uint8_t* data, uint16_t bytes, rtp_ext_video_layers_allocation_t* ext) {
		return 0 == rtp_ext_video_layers_allocation_parse(data, bytes, ext);
	}
	static int Write(uint8_t* data, uint16_t bytes, const rtp_ext_video_layers_allocation_t* ext) {
		return rtp_ext_video_layers_allocation_write(data, bytes, ext);
	}
};

#endif /* !_rtp_header_extension_h_ */
