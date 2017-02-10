#ifndef _mov_format_h_
#define _mov_format_h_

#define MOV_FOURCC(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

#define MOV_AVC1 MOV_FOURCC('a', 'v', 'c', '1') // H.264
#define MOV_MP4V MOV_FOURCC('m', 'p', '4', 'v') // MPEG-4 Video
#define MOV_MP4A MOV_FOURCC('m', 'p', '4', 'a') // AAC

#endif /* !_mov_format_h_ */
