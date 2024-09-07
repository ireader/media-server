#ifndef _xiph_flac_h_
#define _xiph_flac_h_

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct flac_streaminfo_t
{
    uint16_t min_block_size; // in samples
    uint16_t max_block_size;
    uint32_t min_frame_size; // 24-bits, in bytes
    uint32_t max_frame_size;
    uint64_t sample_rate : 20; // 20-bits, [0, 655350Hz]
    uint64_t channels : 3; // (number of channels)-1, 3-bits, [1, 8]
    uint64_t bits_per_sample : 5; // (bits per sample)-1, 5-bits, [4, 32]
    uint64_t samples : 36; // total samples in stream
    uint8_t signature[16]; // MD5 signature of the unencoded audio data
};

/// @return >0-ok, <=0-error
int flac_streaminfo_save(const struct flac_streaminfo_t* flac, uint8_t* data, size_t bytes);
/// @return >0-ok, <=0-error
int flac_streaminfo_load(const uint8_t* data, size_t bytes, struct flac_streaminfo_t* flac);

#if defined(__cplusplus)
}
#endif
#endif /* !_xiph_flac_h_ */
