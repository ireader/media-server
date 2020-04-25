#include "mpeg-ts-proto.h"
#include "mpeg-util.h"

void pcr_write(uint8_t *ptr, int64_t pcr)
{
	int64_t pcr_base = pcr / 300;
	int64_t pcr_ext = pcr % 300;

	ptr[0] = (pcr_base >> 25) & 0xFF;
	ptr[1] = (pcr_base >> 17) & 0xFF;
	ptr[2] = (pcr_base >> 9) & 0xFF;
	ptr[3] = (pcr_base >> 1) & 0xFF;
	ptr[4] = ((pcr_base & 0x01) << 7) | 0x7E | ((pcr_ext>>8) & 0x01);
	ptr[5] = pcr_ext & 0xFF;
}

int mpeg_stream_type_video(int codecid)
{
    switch (codecid)
    {
    case PSI_STREAM_H264:
    case PSI_STREAM_H265:
    case PSI_STREAM_MPEG1:
    case PSI_STREAM_MPEG2:
    case PSI_STREAM_MPEG4:
    case PSI_STREAM_VIDEO_VC1:
    case PSI_STREAM_VIDEO_SVAC:
    case PSI_STREAM_VIDEO_DIRAC:
        return 1;
    default:
        return 0;
    }
}

int mpeg_stream_type_audio(int codecid)
{
    switch (codecid)
    {
    case PSI_STREAM_AAC:
    case PSI_STREAM_MPEG4_AAC:
    case PSI_STREAM_MPEG4_AAC_LATM:
    case PSI_STREAM_AUDIO_MPEG1:
    case PSI_STREAM_MP3:
    case PSI_STREAM_AUDIO_AC3:
    case PSI_STREAM_AUDIO_DTS:
    case PSI_STREAM_AUDIO_EAC3:
    case PSI_STREAM_AUDIO_SVAC:
    case PSI_STREAM_AUDIO_G711A:
    case PSI_STREAM_AUDIO_G711U:
    case PSI_STREAM_AUDIO_G722:
    case PSI_STREAM_AUDIO_G723:
    case PSI_STREAM_AUDIO_G729:
        return 1;
    default:
        return 0;
    }
}
