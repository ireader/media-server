#include "xiph-flac.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// https://xiph.org/flac/format.html#metadata_block_data
/*
STREAM
-------------------------------------------------------------------------------------------------------------------------
<32>			"fLaC", the FLAC stream marker in ASCII, meaning byte 0 of the stream is 0x66, followed by 0x4C 0x61 0x43
METADATA_BLOCK	This is the mandatory STREAMINFO metadata block that has the basic properties of the stream
METADATA_BLOCK*	Zero or more metadata blocks
FRAME+			One or more audio frames


METADATA_BLOCK
-------------------------------------------------------------------------------------------------------------------------
METADATA_BLOCK_HEADER	A block header that specifies the type and size of the metadata block data.
METADATA_BLOCK_DATA


METADATA_BLOCK_HEADER
-------------------------------------------------------------------------------------------------------------------------
<1>		Last-metadata-block flag: '1' if this block is the last metadata block before the audio blocks, '0' otherwise.
<7>		BLOCK_TYPE
		0 : STREAMINFO
		1 : PADDING
		2 : APPLICATION
		3 : SEEKTABLE
		4 : VORBIS_COMMENT
		5 : CUESHEET
		6 : PICTURE
		7-126 : reserved
		127 : invalid, to avoid confusion with a frame sync code
<24>	Length (in bytes) of metadata to follow (does not include the size of the METADATA_BLOCK_HEADER)


METADATA_BLOCK_DATA
-------------------------------------------------------------------------------------------------------------------------
METADATA_BLOCK_STREAMINFO
|| METADATA_BLOCK_PADDING
|| METADATA_BLOCK_APPLICATION
|| METADATA_BLOCK_SEEKTABLE
|| METADATA_BLOCK_VORBIS_COMMENT
|| METADATA_BLOCK_CUESHEET
|| METADATA_BLOCK_PICTURE					The block data must match the block type in the block header.


METADATA_BLOCK_STREAMINFO
-------------------------------------------------------------------------------------------------------------------------
<16>	The minimum block size (in samples) used in the stream.
<16>	The maximum block size (in samples) used in the stream. (Minimum blocksize == maximum blocksize) implies a fixed-blocksize stream.
<24>	The minimum frame size (in bytes) used in the stream. May be 0 to imply the value is not known.
<24>	The maximum frame size (in bytes) used in the stream. May be 0 to imply the value is not known.
<20>	Sample rate in Hz. Though 20 bits are available, the maximum sample rate is limited by the structure of frame headers to 655350Hz. Also, a value of 0 is invalid.
<3>		(number of channels)-1. FLAC supports from 1 to 8 channels
<5>		(bits per sample)-1. FLAC supports from 4 to 32 bits per sample.
<36>	Total samples in stream. 'Samples' means inter-channel sample, i.e. one second of 44.1Khz audio will have 44100 samples regardless of the number of channels. A value of zero here means the number of total samples is unknown.
<128>	MD5 signature of the unencoded audio data. This allows the decoder to determine if an error exists in the audio data even when the error does not result in an invalid bitstream.

NOTES
FLAC specifies a minimum block size of 16 and a maximum block size of 65535, meaning the bit patterns corresponding to the numbers 0-15 in the minimum blocksize and maximum blocksize fields are invalid.
*/


int flac_streaminfo_save(const struct flac_streaminfo_t* flac, uint8_t* data, size_t bytes)
{
    if (bytes < 34 + 4 + 4)
        return -1;

    memcpy(data, "fLaC", 4);
    data[4] = 0x00 | 0; // BLOCK_TYPE: STREAMINFO
    data[5] = 0;
    data[6] = 0;
    data[7] = 34; // stream info block length

    // METADATA_BLOCK_STREAMINFO
    data[8] = (uint8_t)(flac->min_block_size >> 8);
    data[9] = (uint8_t)flac->min_block_size;
    data[10] = (uint8_t)(flac->max_block_size >> 8);
    data[11] = (uint8_t)flac->max_block_size;
    data[12] = (uint8_t)(flac->min_frame_size >> 16);
    data[13] = (uint8_t)(flac->min_frame_size >> 8);
    data[14] = (uint8_t)(flac->min_frame_size >> 0);
    data[15] = (uint8_t)(flac->max_frame_size >> 16);
    data[16] = (uint8_t)(flac->max_frame_size >> 8);
    data[17] = (uint8_t)(flac->max_frame_size >> 0);
    data[18] = (uint8_t)(flac->sample_rate >> 12);
    data[19] = (uint8_t)(flac->sample_rate >> 4);
    data[20] = (uint8_t)((flac->sample_rate & 0x0F) << 4) | ((flac->channels & 0x07) << 1) | ((flac->bits_per_sample >> 4) & 0x01);
    data[21] = (uint8_t)((flac->bits_per_sample & 0x0F) << 4) | ((flac->samples >> 32) & 0x0F);
    data[22] = (uint8_t)(flac->samples >> 24);
    data[23] = (uint8_t)(flac->samples >> 16);
    data[24] = (uint8_t)(flac->samples >> 8);
    data[25] = (uint8_t)(flac->samples >> 0);
    memcpy(data + 26, flac->signature, 16);
    return 34 + 4 + 4;
}

int flac_streaminfo_load(const uint8_t* data, size_t bytes, struct flac_streaminfo_t* flac)
{
    int n = 0;
    uint32_t len;
    const uint8_t* ptr;

    ptr = data;

    // FLAC stream marker
    if (bytes > 4 && 0 == memcmp(data, "fLaC", 4))
    {
        n = 4;
        data += 4;
        bytes -= 4;
    }

    // METADATA_BLOCK_HEADER
    for (; bytes > 4; bytes -= 4 + len, data += 4 + len)
    {
        len = (((uint32_t)data[1]) << 16) | (((uint32_t)data[2]) << 8) | (uint32_t)data[3];  // 24bits length
        if (bytes < 4 + len)
            return -1;

        // METADATA_BLOCK_HEADER
        if ((data[0] & 0x7F) != 0 /*STREAMINFO*/)
            continue;

        if (len < 34)
            return -1;

        memset(flac, 0, sizeof(*flac));
        flac->min_block_size = ((uint16_t)data[4] << 8) | data[5];
        flac->max_block_size = ((uint16_t)data[6] << 8) | data[7];
        flac->min_frame_size = ((uint32_t)data[8] << 16) | ((uint32_t)data[9] << 8) | data[10];
        flac->max_frame_size = ((uint32_t)data[11] << 16) | ((uint32_t)data[12] << 8) | data[13];
        flac->sample_rate = (((uint32_t)data[14] << 16) | ((uint32_t)data[15] << 8) | data[16]) >> 4;
        flac->channels = ((data[16] >> 1) & 0x07);
        flac->bits_per_sample = ((data[16] & 0x01) << 4) + (data[17] >> 4);
        flac->samples = ((((uint64_t)data[17]) & 0x0F) << 32) | (((uint64_t)data[18] << 24) | ((uint64_t)data[19] << 16) | ((uint64_t)data[20] << 8) | (uint64_t)data[21]);
        memcpy(flac->signature, data + 22, 16);
        return (int)(data + 4 + len - ptr);
    }

    return -1;
}

#if defined(DEBUG) || defined(_DEBUG)
void flac_streaminfo_test(void)
{
    uint8_t data[42];
    const uint8_t src[] = {0x66, 0x4C, 0x61, 0x43, 0x00, 0x00, 0x00, 0x22, 0x04, 0x80, 0x04, 0x80, 0x00, 0x06, 0x72, 0x00, 0x17, 0xF2, 0x17, 0x70, 0x03, 0x70, 0x00, 0x3A, 0x69, 0x80, 0xE5, 0xD1, 0x00, 0xC6, 0x3F, 0x51, 0x88, 0x90, 0x0C, 0x66, 0xB6, 0xA6, 0xA0, 0x8C, 0xE2, 0xEB, };

    struct flac_streaminfo_t flac;
    assert(sizeof(src) == flac_streaminfo_load(src, sizeof(src), &flac));
    assert(sizeof(src) == flac_streaminfo_save(&flac, data, sizeof(data)));
    assert(0 == memcmp(src, data, sizeof(src)));
}
#endif
