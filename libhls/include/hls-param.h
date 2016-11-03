#ifndef _hls_param_h_
#define _hls_param_h_

// https://developer.apple.com/library/ios/documentation/NetworkingInternet/Conceptual/StreamingMediaGuide/FrequentlyAskedQuestions/FrequentlyAskedQuestions.html
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
