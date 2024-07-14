#include "rtp-ext.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/inband-cn/
/*
Introduction
Comfort noise (CN) is widely used in real time communication, as it significantly reduces the frequency of RTP packets, and thus saves the network bandwidth, when participants in the communication are constantly actively speaking.

One way of deploying CN is through [RFC 3389]. It defines CN as a special payload, which needs to be encoded and decoded independently from the codec(s) applied to active speech signals. This deployment is referred to as outband CN in this context.

Some codecs, for example RFC 6716: Definition of the Opus Audio Codec, implement their own CN schemes. Basically, the encoder can notify that a CN packet is issued and/or no packet needs to be transmitted.

Since CN packets have their particularities, cloud and client may need to identify them and treat them differently. Special treatments on CN packets include but are not limited to

Upon receiving multiple streams of CN packets, choose only one to relay or mix.
Adapt jitter buffer wisely according to the discontinuous transmission nature of CN packets.
While RTP packets that contain outband CN can be easily identified as they bear a different payload type, inband CN cannot. Some codecs may be able to extract the information by decoding the packet, but that depends on codec implementation, not even mentioning that decoding packets is not always feasible. This document proposes using an RTP header extension to signal the inband CN.

RTP header extension format
The inband CN extension can be encoded using either the one-byte or two-byte header defined in [RFC 5285]. Figures 1 and 2 show encodings with each of these header formats.

 0                   1
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  ID   | len=0 |N| noise level |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Figure 1. Encoding Using the One-Byte Header Format

 0                   1                   2
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|      ID       |     len=1     |N| noise level |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Figure 2. Encoding Using the Two-Byte Header Format

Noise level is an optional data. The bit ¡°N¡± being 1 indicates that there is a noise level. The noise level is defined the same way as the audio level in [RFC 6464] and therefore can be used to avoid the Audio Level Header Extension on the same RTP packet. This also means that this level is defined the same as the noise level in [RFC 3389] and therfore can be compared against outband CN.

Further details
The existence of this header extension in an RTP packet indicates that it has inband CN, and therefore it will be used sparsely, and results in very small transmission cost.

The end receiver can utilize this RTP header extension to get notified about an upcoming discontinuous transmission. This can be useful for its jitter buffer management. This RTP header extension signals comfort noise, it can also be used by audio mixer to mix streams wisely. As an example, it can avoid mixing multiple comfort noises together.

Cloud may have the benefits of this RTP header extension as an end receiver, if it does transcoding. It may also utilize this RTP header extension to prioritize RTP packets if it does packet filtering. In both cases, this RTP header extension should not be encrypted.
*/

int rtp_ext_inband_cn_parse(const uint8_t* data, int bytes, uint8_t *level)
{
    assert(1 == bytes);
    if (1 != bytes)
        return -1;

    *level = data[0] & 0x7f;
    return 0;
}

int rtp_ext_inband_cn_write(uint8_t* data, int bytes, uint8_t level)
{
    if (bytes < 1)
        return -1;
    data[0] = level & 0x7f;
    return 1;
}
