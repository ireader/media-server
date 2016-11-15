#ifndef _mov_esds_h_
#define _mov_esds_h_

// http://www.mp4ra.org/object.html
enum mp4_obj_tag {
	MP4_TAG_H264		= 0x21,
	MP4_TAG_HEVC		= 0x23,
	MP4_TAG_AAC			= 0x40,
	MP4_TAG_AAC_MAIN	= 0x66, /* MPEG-2 AAC Main */
	MP4_TAG_AAC_LOW		= 0x67, /* MPEG-2 AAC Low */
	MP4_TAG_AAC_SSR		= 0x68, /* MPEG-2 AAC SSR */
};

#endif /* !_mov_esds_h_ */
