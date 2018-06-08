#include "mpeg-ps-proto.h"
#include "mpeg-util.h"
#include <assert.h>

// 2.5.3.5 System header (p79)
// Table 2-40 ¨C Program stream system header
size_t system_header_read(struct ps_system_header_t *h, const uint8_t* data, size_t bytes)
{
    size_t i, j;
    size_t len;

    if (bytes < 12) return 0;

    assert(0x00 == data[0] && 0x00 == data[1] && 0x01 == data[2] && PES_SID_SYS == data[3]);
    len = (data[4] << 8) | data[5];
    assert(len + 6 <= bytes);

    assert((0x80 & data[6]) == 0x80); // '1xxxxxxx'
    assert((0x01 & data[8]) == 0x01); // 'xxxxxxx1'
    h->rate_bound = ((data[6] & 0x7F) << 15) | (data[7] << 7) | ((data[8] >> 1) & 0x7F);

    h->audio_bound = (data[9] >> 2) & 0x3F;
    h->fixed_flag = (data[9] >> 1) & 0x01;
    h->CSPS_flag = (data[9] >> 0) & 0x01;

    assert((0x20 & data[10]) == 0x20); // 'xx1xxxxx'
    h->system_audio_lock_flag = (data[10] >> 7) & 0x01;
    h->system_video_lock_flag = (data[10] >> 6) & 0x01;
    h->video_bound = data[10] & 0x1F;

    //	assert((0x7F & data[11]) == 0x00); // 'x0000000'
    h->packet_rate_restriction_flag = (data[11] >> 7) & 0x01;

    i = 12;
    for (j = 0; (data[i] & 0x80) == 0x80 && j < sizeof(h->streams) / sizeof(h->streams[0]) && i < bytes; j++)
    {
        h->streams[j].stream_id = data[i++];
        if (h->streams[j].stream_id == PES_SID_EXTENSION) // '10110111'
        {
            assert(data[i] == 0xC0); // '11000000'
            assert((data[i + 1] & 80) == 0); // '1xxxxxxx'
            h->streams[j].stream_id = (h->streams[j].stream_id << 7) | (data[i + 1] & 0x7F);
            assert(data[i + 2] == 0xB6); // '10110110'
            i += 3;
        }

        assert((data[i] & 0xC0) == 0xC0); // '11xxxxxx'
        h->streams[j].buffer_bound_scale = (data[i] >> 5) & 0x01;
        h->streams[j].buffer_size_bound = (data[i] & 0x1F) | data[i + 1];
        i += 2;
    }

    return len + 4 + 2;
}

size_t system_header_write(const struct ps_system_header_t *h, uint8_t *data)
{
    size_t i, j;

    // system_header_start_code
    nbo_w32(data, 0x000001BB);

    // header length
    //put16(data + 4, 6 + h->stream_count*3);

    // rate_bound
    // 1xxxxxxx xxxxxxxx xxxxxxx1
    data[6] = 0x80 | ((h->rate_bound >> 15) & 0x7F);
    data[7] = (h->rate_bound >> 7) & 0xFF;
    data[8] = 0x01 | ((h->rate_bound & 0x7F) << 1);

    // 6-audio_bound + 1-fixed_flag + 1-CSPS_flag
    data[9] = ((h->audio_bound & 0x3F) << 2) | ((h->fixed_flag & 0x01) << 1) | (h->CSPS_flag & 0x01);

    // 1-system_audio_lock_flag + 1-system_video_lock_flag + 1-maker + 5-video_bound
    data[10] = 0x20 | ((h->system_audio_lock_flag & 0x01) << 7) | ((h->system_video_lock_flag & 0x01) << 6) | (h->video_bound & 0x1F);

    // 1-packet_rate_restriction_flag + 7-reserved
    data[11] = 0x7F | ((h->packet_rate_restriction_flag & 0x01) << 7);

    i = 12;
    for (j = 0; j < h->stream_count; j++)
    {
        data[i++] = (uint8_t)h->streams[j].stream_id;
        if (PES_SID_EXTENSION == h->streams[j].stream_id) // '10110111'
        {
            data[i++] = 0xD0; // '11000000'
            data[i++] = h->streams[j].stream_extid & 0x7F; // '0xxxxxxx'
            data[i++] = 0xB6; // '10110110'
        }

        // '11' + 1-P-STD_buffer_bound_scale + 13-P-STD_buffer_size_bound
        // '11xxxxxx xxxxxxxx'
        data[i++] = 0xC0 | ((h->streams[j].buffer_bound_scale & 0x01) << 5) | ((h->streams[j].buffer_size_bound >> 8) & 0x1F);
        data[i++] = h->streams[j].buffer_size_bound & 0xFF;
    }

    // header length
    nbo_w16(data + 4, (uint16_t)(i - 6));
    return i;
}
