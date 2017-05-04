#ifndef _hls_param_h_
#define _hls_param_h_

// https://developer.apple.com/library/content/documentation/NetworkingInternet/Conceptual/StreamingMediaGuide/FrequentlyAskedQuestions/FrequentlyAskedQuestions.html
// https://developer.apple.com/library/ios/documentation/NetworkingInternet/Conceptual/StreamingMediaGuide/FrequentlyAskedQuestions/FrequentlyAskedQuestions.html
// 1. What kinds of encoders are supported?
// H.264 video and AAC audio (HE-AAC or AAC-LC).
// 2. What are the specifics of the video and audio formats supported?
// Video: H.264 Baseline Level 3.0, Baseline Level 3.1, Main Level 3.1, and High Profile Level 4.1.
// Audio: HE-AAC or AAC-LC up to 48 kHz, stereo audio
//        MP3 (MPEG-1 Audio Layer 3) 8 kHz to 48 kHz, stereo audio
//        AC-3 (for Apple TV, in pass-through mode only)

// 3. What duration should media files be?
// A duration of 10 seconds of media per file seems to strike a reasonable balance for most broadcast content.
// http://devimages.apple.com/iphone/samples/bipbop/bipbopall.m3u8
#define HLS_DURATION 10 // 10s, from Apple recommendation

// 4. How many files should be listed in the index file during a continuous, ongoing session?
// The normal recommendation is 3, but the optimum number may be larger.
#define HLS_FILE_NUM 8

#define HLS_BLOCK_SIZE (188*1024)

#define HLS_M3U8_TYPE   "application/vnd.apple.mpegURL" // HTTP Content-Type
#define HLS_TS_TYPE     "video/MP2T" // HTTP Content-Type

#define PTS_NO_VALUE INT64_MIN //(int64_t)0x8000000000000000L

#endif /* !_hls_param_h_ */
