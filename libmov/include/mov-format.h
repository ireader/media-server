#ifndef _mov_format_h_
#define _mov_format_h_

#define MOV_FOURCC(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

#define MOV_AVC1 MOV_FOURCC('a', 'v', 'c', '1') // H.264
#define MOV_HEVC MOV_FOURCC('h', 'e', 'v', 'c') // H.265
#define MOV_MP4V MOV_FOURCC('m', 'p', '4', 'v') // MPEG-4 Video
#define MOV_MP4A MOV_FOURCC('m', 'p', '4', 'a') // AAC

/// MOV flags
#define MOV_FLAG_FASTSTART 0x00000001

/// MOV av stream flag
#define MOV_AV_FLAG_KEYFREAME 0x0001

#endif /* !_mov_format_h_ */
