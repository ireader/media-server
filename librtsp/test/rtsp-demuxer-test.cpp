#include <stdio.h>
#include <assert.h>
#include "rtsp-demuxer.h"

static int rtsp_demuxer_test_onpacket(void* param, int track, int payload, const char* encoding, int64_t pts, int64_t dts, const void* data, int bytes, int flags)
{
    return 0;
}

void rstp_demuxer_test(int payload, const char* encoding, uint16_t seq, uint32_t ssrc, const char* rtpfile)
{
    uint8_t buffer[1500];
    FILE* fp = fopen(rtpfile, "rb");
    assert(fp);
 
    struct rtsp_demuxer_t* demuxer = rtsp_demuxer_create(90000, payload, encoding, "", rtsp_demuxer_test_onpacket, NULL);
    rtsp_demuxer_rtpinfo(demuxer, seq, ssrc);
    
    while (1)
    {
        uint8_t s2[2];
        if (2 != fread(s2, 1, 2, fp))
            break;

        int size = (s2[0] << 8) | s2[1];
        assert(size < sizeof(buffer));
        assert(size == (int)fread(buffer, 1, size, fp));
        assert(rtsp_demuxer_input(demuxer, buffer, size) >= 0);
    }

    rtsp_demuxer_destroy(demuxer);
    fclose(fp);
}
