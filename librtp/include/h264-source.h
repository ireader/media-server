#ifndef _h264_source_h_
#define _h264_source_h_

typedef void (*OnH264)(void* param, unsigned char nal, const void* data, int bytes);

void* h264_source_create(void* queue, OnH264 cb, void* param);
int h264_source_destroy(void* h264);
int h264_source_process(void* h264);

#endif /* !_h264_source_h_ */
