#ifndef rtp_sender_h
#define rtp_sender_h

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rtp_sender_t
{
    void* encoder;
    void* rtp;
    
    int payload;
    char encoding[16];
    
    uint16_t seq;
    uint32_t ssrc;
    uint32_t timestamp;
    uint32_t frequency; // default video: 90000
    uint32_t bandwidth; // default video: 2Mb, audio: 128Kb
    
    uint8_t buffer[2 * 1024]; // for sdp and rtp packet
    
    /// @return 0-ok, other-error
    int (*onpacket)(void* param, const void *packet, int bytes, uint32_t timestamp, int flags);
    void (*onbye)(void* param); // rtcp bye msg
    void* param;
};

/// @param[in] proto RTP/AVP, see more @librtsp/include/sdp-utils.h
int rtp_sender_init_video(struct rtp_sender_t* s, const char* proto, unsigned short port, int payload, const char* encoding, int frequence, const void* extra, size_t bytes);
int rtp_sender_init_audio(struct rtp_sender_t* s, const char* proto, unsigned short port, int payload, const char* encoding, int sample_rate, int channel_count, const void* extra, size_t bytes);

int rtp_sender_destroy(struct rtp_sender_t* s);

#ifdef __cplusplus
}
#endif
#endif /* rtp_sender_h */
