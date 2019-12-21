#ifndef _mov_format_h_
#define _mov_format_h_

// ISO/IEC 14496-1:2010(E) 7.2.6.6 DecoderConfigDescriptor (p48)
// MPEG-4 systems ObjectTypeIndication
// http://www.mp4ra.org/object.html
#define MOV_OBJECT_TEXT		0x08 // Text Stream
#define MOV_OBJECT_MP4V		0x20 // Visual ISO/IEC 14496-2 (c)
#define MOV_OBJECT_H264		0x21 // Visual ITU-T Recommendation H.264 | ISO/IEC 14496-10 
#define MOV_OBJECT_HEVC		0x23 // Visual ISO/IEC 23008-2 | ITU-T Recommendation H.265
#define MOV_OBJECT_AAC		0x40 // Audio ISO/IEC 14496-3
#define MOV_OBJECT_MP2V		0x60 // Visual ISO/IEC 13818-2 Simple Profile
#define MOV_OBJECT_AAC_MAIN	0x66 // MPEG-2 AAC Main
#define MOV_OBJECT_AAC_LOW	0x67 // MPEG-2 AAC Low
#define MOV_OBJECT_AAC_SSR	0x68 // MPEG-2 AAC SSR
#define MOV_OBJECT_MP3		0x69 // Audio ISO/IEC 13818-3
#define MOV_OBJECT_MP1V		0x6A // Visual ISO/IEC 11172-2
#define MOV_OBJECT_MP1A		0x6B // Audio ISO/IEC 11172-3
#define MOV_OBJECT_JPEG		0x6C // Visual ISO/IEC 10918-1 (JPEG)
#define MOV_OBJECT_PNG		0x6D // Portable Network Graphics (f)
#define MOV_OBJECT_JPEG2000	0x6E // Visual ISO/IEC 15444-1 (JPEG 2000)
#define MOV_OBJECT_G719		0xA8 // ITU G.719 Audio
#define MOV_OBJECT_OPUS		0xAD // Opus audio
#define MOV_OBJECT_G711a	0xFD // ITU G.711 alaw
#define MOV_OBJECT_G711u	0xFE // ITU G.711 ulaw
#define MOV_OBJECT_AV1		0xFF // AV1: https://aomediacodec.github.io/av1-isobmff

/// MOV flags
#define MOV_FLAG_FASTSTART	0x00000001
#define MOV_FLAG_SEGMENT	0x00000002 // fmp4_writer only

/// MOV av stream flag
#define MOV_AV_FLAG_KEYFREAME 0x0001

#endif /* !_mov_format_h_ */
