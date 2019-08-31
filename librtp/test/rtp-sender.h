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
    uint32_t frequency;
    uint32_t bandwidth;
    
    uint8_t buffer[2 * 1024];
    
    int (*send)(void* param, const void *packet, int bytes);
    void (*onbye)(void* param); // rtcp bye msg
    void* param;
};

int rtp_sender_init_video(struct rtp_sender_t* s, int port, int payload, const char* encoding, int width, int height, const void* extra, size_t bytes);
int rtp_sender_init_audio(struct rtp_sender_t* s, int port, int payload, const char* encoding, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* rtp_sender_h */
