#include "mpeg-types.h"
#include "mpeg-util.h"
#include "mpeg-proto.h"
#include <assert.h>
#include <string.h>

#define H266_NAL_IDR_W_RADL	7
#define H266_NAL_RSV_IRAP	11
#define H266_NAL_OPI		12
#define H266_NAL_DCI		13
#define H266_NAL_VPS		14
#define H266_NAL_SPS		15
#define H266_NAL_PPS		16
#define H266_PREFIX_APS_NUT 17
#define H266_SUFFIX_APS_NUT 18
#define H266_PH_NUT         19
#define H266_NAL_AUD		20
#define H266_NAL_PREFIX_SEI 23
#define H266_NAL_SUFFIX_SEI 24

/// @return 0-not find, 1-find ok
int mpeg_h266_start_with_access_unit_delimiter(const uint8_t* p, size_t bytes)
{
    int i;
    uint8_t nalu;
    i = mpeg_h264_find_nalu(p, bytes, NULL);
    if (-1 == i)
        return 0;

    assert(i > 0);
    nalu = (p[i + 1] >> 3) & 0x1f;
    return H266_NAL_AUD == nalu ? 1 : 0;
}

static int mpeg_h266_is_new_access_unit(const uint8_t* nalu, size_t bytes)
{
    uint8_t nal_type;
    uint8_t nuh_layer_id;

    if (bytes < 3)
        return 0;

    nal_type = (nalu[1] >> 3) & 0x1f;
    nuh_layer_id = nalu[0] & 0x3F;

    // 7.4.2.4.3 Order of PUs and their association to AUs
    if (H266_NAL_AUD == nal_type || H266_NAL_OPI == nal_type || H266_NAL_DCI == nal_type || H266_NAL_VPS == nal_type || H266_NAL_SPS == nal_type || H266_NAL_PPS == nal_type ||
        (nuh_layer_id == 0 && (H266_PREFIX_APS_NUT == nal_type || H266_PH_NUT == nal_type || H266_NAL_PREFIX_SEI == nal_type || 
                                26 == nal_type || (28 <= nal_type && nal_type <= 29))))
        return 1;

    // 7.4.2.4.4 Order of NAL units and coded pictures and their association to PUs
    if (nal_type < H266_NAL_OPI)
    {
        //sh_picture_header_in_slice_header_flag == 1
        return (nalu[2] & 0x80) ? 1 : 0;
    }

    return 0;
}

int mpeg_h266_find_new_access_unit(const uint8_t* data, size_t bytes, int* vcl)
{
    int n;
    size_t leading;
    uint8_t nal_type;
    const uint8_t* p, * end;

    end = data + bytes;
    for (p = data; p && p < end; p += n)
    {
        n = mpeg_h264_find_nalu(p, end - p, &leading);
        if (n < 1)
            return -1;

        nal_type = (p[n+1] >> 3) & 0x1f;
        if (*vcl > 0 && mpeg_h266_is_new_access_unit(p + n, end - p - n))
        {
            return (int)(p - data + n - leading);
        }
        else if (nal_type < H266_NAL_OPI)
        {
            *vcl = (H266_NAL_IDR_W_RADL <= nal_type && nal_type <= H266_NAL_RSV_IRAP) ? MPEG_VCL_IDR : MPEG_VCL_P;
        }
        else if (PES_SID_START == p[n])
        {
            // pes data loss ???
            *vcl = MPEG_VCL_CORRUPT; // for assert
            return (int)(p - data + n - leading);
        }
        else
        {
            // nothing to do
        }
    }

    return -1;
}
