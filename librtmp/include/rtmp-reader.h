#ifndef _rtmp_reader_h_
#define _rtmp_reader_h_

#if defined(__cplusplus)
extern "C" {
#endif

	void* rtmp_reader_create(const char* url);
	void rtmp_reader_destroy(void* rtmp);

	int rtmp_reader_read(void* rtmp, void* packet, int bytes);
#if defined(__cplusplus)
}
#endif
#endif /* !* _rtmp_reader_h_ */
