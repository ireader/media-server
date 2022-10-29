/*
 * Copyright (c) 2021 ireader <tao3@outlook.com>. All rights reserved.
 */

#ifndef _rtp_ext_h_
#define _rtp_ext_h_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml
// https://www.iana.org/assignments/rtcp-xr-block-types/rtcp-xr-block-types.xhtml

/*
RTCP Control Packet Types (PT)
   Value  Abbrev.  Name                                 Reference
   0               Reserved
   1-191           Unassigned
   192             Reserved (Historic-FIR)              [RFC2032]
   193             Reserved (Historic-NACK)             [RFC2032]
   194     SMPTETC SMPTE time-code mapping              [RFC5484]
   195     IJ      Extended inter-arrival jitter report [RFC5450]
   196-199         Unassigned
   200     SR      sender report                        [RFC3550]
   201     RR      receiver report                      [RFC3550]
   202     SDES    source description                   [RFC3550]
   203     BYE     goodbye                              [RFC3550]
   204     APP     application-defined                  [RFC3550]
   205     RTPFB   Generic RTP Feedback                 [RFC4585]
   206     PSFB    Payload-specific                     [RFC4585]
   207     XR      extended report                      [RFC3611]
   208     AVB     AVB RTCP packet                      ["Standard for Layer 3 Transport Protocol for Time Sensitive Applications in Local Area
                                                        Networks." Work in progress.]
   209     RSI     Receiver Summary Information         [RFC5760]
   210     TOKEN   Port Mapping                         [RFC6284]
   211     IDMS    IDMS Settings                        [RFC7272]
   212     RGRS    Reporting Group Reporting Sources    [RFC8861]
   213     SNM     Splicing Notification Message        [RFC8286]
   214-254         Unassigned
   255             Reserved


RTP SDES Item Types
   Value  Abbrev.             Name                            Reference
   0      END                 end of SDES list                [RFC3550]
   1      CNAME               canonical name                  [RFC3550]
   2      NAME                user name                       [RFC3550]
   3      EMAIL               user's electronic mail address  [RFC3550]
   4      PHONE               user's phone number             [RFC3550]
   5      LOC                 geographic user location        [RFC3550]
   6      TOOL                name of application or tool     [RFC3550]
   7      NOTE                notice about the source         [RFC3550]
   8      PRIV                private extensions              [RFC3550]
   9      H323-CADDR          H.323 callable address          [Vineet_Kumar]
   10     APSI                Application Specific Identifier [RFC6776]
   11     RGRP                Reporting Group Identifier      [RFC8861]
   12     RtpStreamId         RTP Stream Identifier           [RFC8852]
   13     RepairedRtpStreamId Repaired RTP Stream Identifier  [RFC8852]
   14     CCID                CLUE CaptId                     [RFC8849]
   15     MID                 Media Identification            [RFC-ietf-mmusic-rfc8843bis-05]
   16-255                     Unassigned


FMT Values for RTPFB Payload Types
   Value Name         Long Name                                            Reference
   1     Generic NACK Generic negative acknowledgement                     [RFC4585]
   2                  Reserved                                             [RFC5104]
   3     TMMBR        Temporary Maximum Media Stream Bit Rate Request      [RFC5104]
   4     TMMBN        Temporary Maximum Media Stream Bit Rate Notification [RFC5104]
   5     RTCP-SR-REQ  RTCP Rapid Resynchronisation Request                 [RFC6051]
   6     RAMS         Rapid Acquisition of Multicast Sessions              [RFC6285]
   7     TLLEI        Transport-Layer Third-Party Loss Early Indication    [RFC6642]
   8     RTCP-ECN-FB  RTCP ECN Feedback                                    [RFC6679]
   9     PAUSE-RESUME Media Pause/Resume                                   [RFC7728]
   10    DBI          Delay Budget Information (DBI)                       [3GPP TS 26.114 v16.3.0][Ozgur_Oyman]
   11    CCFB         RTP Congestion Control Feedback                      [RFC8888]
   12-30              Unassigned
   31    Extension    Reserved for future extensions                       [RFC4585]


FMT Values for PSFB Payload Types
   Value Name      Long Name                                          Reference
   1     PLI       Picture Loss Indication                            [RFC4585]
   2     SLI       Slice Loss Indication                              [RFC4585]
   3     RPSI      Reference Picture Selection Indication             [RFC4585]
   4     FIR       Full Intra Request Command                         [RFC5104]
   5     TSTR      Temporal-Spatial Trade-off Request                 [RFC5104]
   6     TSTN      Temporal-Spatial Trade-off Notification            [RFC5104]
   7     VBCM      Video Back Channel Message                         [RFC5104]
   8     PSLEI     Payload-Specific Third-Party Loss Early Indication [RFC6642]
   9     ROI       Video region-of-interest (ROI)                     [3GPP TS 26.114 v16.3.0][Ozgur_Oyman]
   10    LRR       Layer Refresh Request Command                      [RFC-ietf-avtext-lrr-07]
   11-14           Unassigned
   15    AFB       Application Layer Feedback                         [RFC4585]
   16-30           Unassigned
   31    Extension Reserved for future extensions                     [RFC4585]


RTP Compact Header Extensions
   Extension URI                                Description                               Contact                       Reference
   urn:ietf:params:rtp-hdrext:toffset           Transmission Time offsets                 [Singer]                      [RFC5450]
   urn:ietf:params:rtp-hdrext:smpte-tc          SMPTE time-code mapping                   [Singer]                      [RFC5484]
   urn:ietf:params:rtp-hdrext:ntp-64            Synchronisation metadata: 64-bit          [Thomas_Schierl]              [IETF Audio/Video Transport
                                                timestamp format                                                        Working Group][RFC6051]
   urn:ietf:params:rtp-hdrext:ntp-56            Synchronisation metadata: 56-bit          [Thomas_Schierl]              [IETF Audio/Video Transport
                                                timestamp format                                                        Working Group][RFC6051]
   urn:ietf:params:rtp-hdrext:ssrc-audio-level  Audio Level                               [Jonathan_Lennox]             [RFC6464]
   urn:ietf:params:rtp-hdrext:csrc-audio-level  Mixer-to-client audio level indicators    [Emil_Ivov]                   [RFC6465]
   urn:ietf:params:rtp-hdrext:encrypt           Encrypted extension header element        [Jonathan_Lennox]             [RFC6904]
   urn:3gpp:video-orientation                   Coordination of video orientation (CVO)   [Specifications_Manager_3GPP] [3GPP TS 26.114, version
                                                feature, see clause 6.2.3                                               12.5.0]
                                                Higher granularity (6-bit) coordination                                 [3GPP TS 26.114, version
   urn:3gpp:video-orientation:6                 of video orientation (CVO) feature, see   [Specifications_Manager_3GPP] 12.5.0]
                                                clause 6.2.3
                                                Signalling of the arbitrary                                             [3GPP TS 26.114, version
   urn:3gpp:roi-sent                            region-of-interest (ROI) information for  [Specifications_Manager_3GPP] 13.1.0]
                                                the sent video, see clause 6.2.3.4
                                                Signalling of the predefined                                            [3GPP TS 26.114, version
   urn:3gpp:predefined-roi-sent                 region-of-interest (ROI) information for  [Specifications_Manager_3GPP] 13.1.0]
                                                the sent video, see clause 6.2.3.4
                                                Reserved as base URN for RTCP SDES items
   urn:ietf:params:rtp-hdrext:sdes              that are also defined as RTP compact      Authors of [RFC7941]          [RFC7941]
                                                header extensions.
   urn:ietf:params:rtp-hdrext:splicing-interval Splicing Interval                         [Jinwei_Xia]                  [RFC8286]


RTP SDES Compact Header Extensions
   Extension URI                                          Description                          Contact              Reference
   urn:ietf:params:rtp-hdrext:sdes:cname                  Source Description: Canonical        Authors of [RFC7941] [RFC7941]
                                                          End-Point Identifier (SDES CNAME)
   urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id          RTP Stream Identifier                [Adam_Roach]         [RFC8852]
   urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id RTP Repaired Stream Identifier       [Adam_Roach]         [RFC8852]
   urn:ietf:params:rtp-hdrext:sdes:CaptId                 CLUE CaptId                          [Roni_Even]          [RFC8849]
   urn:ietf:params:rtp-hdrext:sdes:mid                    Media identification                 [IESG]               [RFC-ietf-mmusic-rfc8843bis-05]
*/

enum RTPExtensionType {
    RTP_HDREXT_PADDING = 0,
    RTP_HDREXT_SSRC_AUDIO_LEVEL_ID,             // [rfc6464] urn:ietf:params:rtp-hdrext:ssrc-audio-level
    RTP_HDREXT_CSRC_AUDIO_LEVEL_ID,             // [rfc6465] urn:ietf:params:rtp-hdrext:csrc-audio-level
    RTP_HDREXT_FRAME_MARKING_ID,                // [rfc8852] urn:ietf:params:rtp-hdrext:framemarking
    RTP_HDREXT_SDES_MID_ID,                     // [rfc8852] urn:ietf:params:rtp-hdrext:sdes:mid
    RTP_HDREXT_SDES_RTP_STREAM_ID,              // urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
    RTP_HDREXT_SDES_REPAIRED_RTP_STREAM_ID,     // urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
    RTP_HDREXT_TOFFSET_ID,                      // [rfc5450] urn:ietf:params:rtp-hdrext:toffset
    RTP_HDREXT_VIDEO_ORIENTATION_ID,            // urn:3gpp:video-orientation (http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/ts_126114v120700p.pdf)
    RTP_HDREXT_ABSOLUTE_SEND_TIME_ID,           // http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
    RTP_HDREXT_ABSOLUTE_CAPTURE_TIME_ID,        // http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time
    RTP_HDREXT_TRANSPORT_WIDE_CC_ID_01,         // http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
    RTP_HDREXT_TRANSPORT_WIDE_CC_ID,            // http://www.webrtc.org/experiments/rtp-hdrext/transport-wide-cc-02
    RTP_HDREXT_VIDEO_TIMING_ID,                 // http://www.webrtc.org/experiments/rtp-hdrext/video-timing
    RTP_HDREXT_PLAYOUT_DELAY_ID,                // http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
    RTP_HDREXT_ONE_BYTE_RESERVED,
    RTP_HDREXT_COLOR_SPACE_ID,                  // http://www.webrtc.org/experiments/rtp-hdrext/color-space
    RTP_HDREXT_VIDEO_CONTENT_TYPE_ID,           // http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
    RTP_HDREXT_INBAND_CN_ID,                    // http://www.webrtc.org/experiments/rtp-hdrext/inband-cn
    RTP_HDREXT_VIDEO_FRAME_TRACKING_ID,         // http://www.webrtc.org/experiments/rtp-hdrext/video-frame-tracking-id
    RTP_HDREXT_VIDEO_LAYERS_ALLOCATION_ID,      // http://www.webrtc.org/experiments/rtp-hdrext/video-layers-allocation00
    //RTP_HDREXT_GENERIC_FRAME_DESCRIPTOR_00,     // http://www.webrtc.org/experiments/rtp-hdrext/generic-frame-descriptor-00
    //RTP_HDREXT_GENERIC_FRAME_DESCRIPTOR_02,     // http://www.webrtc.org/experiments/rtp-hdrext/generic-frame-descriptor-02
    //RTP_HDREXT_ENCRYPT,                         // [rfc6904] urn:ietf:params:rtp-hdrext:encrypt

    RTP_HDREXT_NUM
};

enum
{
    RTP_HDREXT_PROFILE_ONE_BYTE         = 0xBEDE,
    RTP_HDREXT_PROFILE_TWO_BYTE         = 0x1000,
    RTP_HDREXT_PROFILE_TWO_BYTE_FILTER  = 0xFFF0,
};

enum { RTP_VIDEO_CONTENT_TYPE_UNSPECIFIED = 0, RTP_VIDEO_CONTENT_TYPE_SCREENSHARE };

struct rtp_ext_uri_t
{
    uint8_t id;
    const char* uri;
};

struct rtp_ext_data_t
{
    uint32_t id : 8;
    uint32_t len : 8; // bytes
    uint32_t off : 16; // offset
};

struct rtp_ext_absolute_capture_time_t
{
    uint64_t timestamp; // absolute capture timestamp
    uint64_t offset; // estimated capture clock offset
};

struct rtp_ext_transport_wide_cc_t
{
    uint32_t seq : 16;
    uint32_t t : 1;
    uint32_t count : 15;
};

struct rtp_ext_video_orientation_t
{
    int camera; // 1-Back-facing camera, 0-Front-facing camera
    int flip; // 1-Horizontal flip operation
    int rotaion; // 0/90/180/270
};

struct rtp_ext_video_timing_t
{
    int flags; // 0x01-extension is set due to timer, 0x02-extension is set because the frame is larger than usual
    uint16_t encode_start;
    uint16_t encode_finish;
    uint16_t packetization_complete;
    uint16_t last_packet_left_the_pacer;
    uint16_t network_timestamp;
    uint16_t network_timestamp2;
};

struct rtp_ext_playout_delay_t
{
    uint16_t min_delay;
    uint16_t max_delay;
};

struct rtp_ext_color_space_t
{
    uint8_t primaries; // Color primaries value according to ITU-T H.273 Table 2.
    uint8_t transfer; // Transfer characteristic value according to ITU-T H.273 Table 3.
    uint8_t matrix; // Matrix coefficients value according to ITU-T H.273 Table 4.
    uint8_t range_chroma_siting; // https://www.webmproject.org/docs/container/#colour

    // HDR metadata(tow-byte RTP header extension)
    uint16_t luminance_max; // Luminance max, specified in nits, where 1 nit = 1 cd/m2. (16-bit unsigned integer)
    uint16_t luminance_min; // Luminance min, scaled by a factor of 10000 and specified in the unit 1/10000 nits. (16-bit unsigned integer)
    uint32_t mastering_metadata_primary_red; // CIE 1931 xy chromaticity coordinates of the primary red, scaled by a factor of 50000. (2x 16-bit unsigned integers)
    uint32_t mastering_metadata_primary_green; // CIE 1931 xy chromaticity coordinates of the primary green, scaled by a factor of 50000. (2x 16-bit unsigned integers)
    uint32_t mastering_metadata_primary_blue; // CIE 1931 xy chromaticity coordinates of the primary blue, scaled by a factor of 50000. (2x 16-bit unsigned integers)
    uint32_t mastering_metadata_primary_white; // CIE 1931 xy chromaticity coordinates of the white point, scaled by a factor of 50000. (2x 16-bit unsigned integers)
    uint16_t max_content_light_level; // Max content light level, specified in nits. (16-bit unsigned integer)
    uint16_t max_frame_average_light_level; // Max frame average light level, specified in nits. (16-bit unsigned integer)
};

struct rtp_ext_frame_marking_t
{
    uint32_t s : 1;		/* Start of Frame */
    uint32_t e : 1;		/* End of Frame */
    uint32_t i : 1;		/* Independent Frame */
    uint32_t d : 1;		/* Discardable Frame */
    uint32_t b : 1;		/* Base Layer Sync */
    uint32_t tid : 3;   // The temporal layer ID of current frame
    uint32_t lid : 8;
    uint32_t tl0_pic_idx : 8; // 8 bits temporal layer zero index
};

struct rtp_ext_video_layers_allocation_t
{
    uint8_t rid;
};

/// count: RTP_HDREXT_NUM-1(skip padding)
/// @return ext id/uri
const struct rtp_ext_uri_t* rtp_ext_list();
const struct rtp_ext_uri_t* rtp_ext_find_uri(const char* uri);

/// @param[out] exts parsed rtpext header payload offset/bytes (MUST memset(exts, 0, sizeof(exts)));
/// @return 0-ok, other-error
int rtp_ext_read(uint16_t profile, const uint8_t* data, int bytes, struct rtp_ext_data_t exts[256]);

/// @param[in] profile RTP_HDREXT_PROFILE_ONE_BYTE/RTP_HDREXT_PROFILE_TWO_BYTE, 0-auto(detect one/two byte by length)
/// @param[in] count rtp hdrext item count(exts)
/// @return >0-ok, other-error
int rtp_ext_write(uint16_t profile, const uint8_t* extension, const struct rtp_ext_data_t *exts, int count, uint8_t* data, int bytes);

/// @param[in] n should be at least bytes + 1
/// @return 0-ok, other-error
int rtp_ext_string_parse(const uint8_t* data, int bytes, char* v, int n);
/// @return write bytes
int rtp_ext_string_write(uint8_t* data, int bytes, const char* v, int n);

/// @param[out] activity 0-inactivity, 1-activity
/// @return 0-ok, other-error
int rtp_ext_ssrc_audio_level_parse(const uint8_t* data, int bytes, uint8_t* activity, uint8_t* level);
/// @return write bytes
int rtp_ext_ssrc_audio_level_write(uint8_t* data, int bytes, uint8_t activity, uint8_t level);
/// @return 0-ok, other-error
int rtp_ext_csrc_audio_level_parse(const uint8_t* data, int bytes, uint8_t levels[], int num);
/// @return write bytes
int rtp_ext_csrc_audio_level_write(uint8_t* data, int bytes, const uint8_t levels[], int num);
/// @return 0-ok, other-error
int rtp_ext_frame_marking_parse(const uint8_t* data, int bytes, struct rtp_ext_frame_marking_t* ext);
/// @return write bytes
int rtp_ext_frame_marking_write(uint8_t* data, int bytes, const struct rtp_ext_frame_marking_t* ext);
//int rtp_ext_sdes_mid(void* param, const uint8_t* data, int bytes);
//int rtp_ext_sdes_rtp_stream_id(void* param, const uint8_t* data, int bytes);
//int rtp_ext_sdes_repaired_rtp_stream_id(void* param, const uint8_t* data, int bytes);
/// @param[out] timestamp rtp time
/// @return 0-ok, other-error
int rtp_ext_toffset_parse(const uint8_t* data, int bytes, uint32_t* timestamp);
/// @return write bytes
int rtp_ext_toffset_write(uint8_t* data, int bytes, uint32_t timestamp);
/// @param[out] rotaion 0/90/180/270
/// @return 0-ok, other-error
int rtp_ext_video_orientation_parse(const uint8_t* data, int bytes, struct rtp_ext_video_orientation_t* ext);
/// @return write bytes
int rtp_ext_video_orientation_write(uint8_t* data, int bytes, const struct rtp_ext_video_orientation_t* ext);
/// @param[out] timestamp in millisecond
/// @return 0-ok, other-error
int rtp_ext_abs_send_time_parse(const uint8_t* data, int bytes, uint64_t* timestamp);
/// @return write bytes
int rtp_ext_abs_send_time_write(uint8_t* data, int bytes, uint64_t timestamp);
/// @return 0-ok, other-error
int rtp_ext_absolute_capture_time_parse(const uint8_t* data, int bytes, struct rtp_ext_absolute_capture_time_t* ext);
/// @return write bytes
int rtp_ext_absolute_capture_time_write(uint8_t* data, int bytes, const struct rtp_ext_absolute_capture_time_t* ext);
/// @return 0-ok, other-error
int rtp_ext_transport_wide_cc_parse(const uint8_t* data, int bytes, struct rtp_ext_transport_wide_cc_t* ext);
/// @return write bytes(2-v1, 4-v2)
int rtp_ext_transport_wide_cc_write(uint8_t* data, int bytes, const struct rtp_ext_transport_wide_cc_t* ext);
/// @return 0-ok, other-error
int rtp_ext_video_timing_parse(const uint8_t* data, int bytes, struct rtp_ext_video_timing_t* ext);
/// @return write bytes
int rtp_ext_video_timing_write(uint8_t* data, int bytes, const struct rtp_ext_video_timing_t* ext);
/// @return 0-ok, other-error
int rtp_ext_playout_delay_parse(const uint8_t* data, int bytes, struct rtp_ext_playout_delay_t* ext);
/// @return write bytes
int rtp_ext_playout_delay_write(uint8_t* data, int bytes, const struct rtp_ext_playout_delay_t* ext);
/// @return 0-ok, other-error
int rtp_ext_color_space_parse(const uint8_t* data, int bytes, struct rtp_ext_color_space_t* ext);
/// @return write bytes
int rtp_ext_color_space_write(uint8_t* data, int bytes, const struct rtp_ext_color_space_t* ext);
/// @return 0-ok, other-error
int rtp_ext_video_content_type_parse(const uint8_t* data, int bytes, uint8_t* ext);
/// @return write bytes
int rtp_ext_video_content_type_write(uint8_t* data, int bytes, uint8_t ext);
/// @return 0-ok, other-error
int rtp_ext_inband_cn_parse(const uint8_t* data, int bytes, uint8_t* level);
/// @return write bytes
int rtp_ext_inband_cn_write(uint8_t* data, int bytes, uint8_t level);
/// @return 0-ok, other-error
int rtp_ext_video_frame_tracking_id_parse(const uint8_t* data, int bytes, uint16_t* id);
/// @return write bytes
int rtp_ext_video_frame_tracking_id_write(uint8_t* data, int bytes, uint16_t id);
/// @return 0-ok, other-error
int rtp_ext_video_layers_allocation_parse(const uint8_t* data, int bytes, struct rtp_ext_video_layers_allocation_t* ext);
/// @return write bytes
int rtp_ext_video_layers_allocation_write(uint8_t* data, int bytes, const struct rtp_ext_video_layers_allocation_t* ext);

#ifdef __cplusplus
}
#endif
#endif /* !_rtp_ext_h_ */
