// https://aomediacodec.github.io/av1-rtp-spec/#7-payload-format-parameters

#include "aom-av1.h"
#include "sdp-payload.h"
#include "base64.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int sdp_av1(uint8_t *data, int bytes, const char* proto, unsigned short port, int payload, int frequence, const void* extra, int extra_size)
{
    /*
    m=video 49170 RTP/AVPF 98
    a=rtpmap:98 AV1/90000
    a=fmtp:98 profile=2; level-idx=8; tier=1;
     */
    static const char* pattern =
        "m=video %hu %s %d\n"
        "a=rtpmap:%d AV1/90000\n"
        //"a=rtpmap:%d AV1X/90000\n" // https://bugs.chromium.org/p/webrtc/issues/detail?id=11042
        "a=fmtp:%d profile=%u;level-idx=%u;tier=%u";

    int r, n;
    struct aom_av1_t av1;

    assert(90000 == frequence);
    r = aom_av1_codec_configuration_record_load((const uint8_t*)extra, extra_size, &av1);
    if (r < 0) return r;

    payload = 35; // https://bugs.chromium.org/p/webrtc/issues/detail?id=11042
    n = snprintf((char*)data, bytes, pattern, port, proto && *proto ? proto : "RTP/AVP", payload, payload, payload,
                 (unsigned int)av1.seq_profile, (unsigned int)av1.seq_level_idx_0, (unsigned int)av1.seq_tier_0);

    if (n < bytes)
        data[n++] = '\n';
    return n;
}
