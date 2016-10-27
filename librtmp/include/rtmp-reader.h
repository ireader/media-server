#ifndef _rtmp_reader_h_
#define _rtmp_reader_h_

#if defined(__cplusplus)
extern "C" {
#endif

	void* rtmp_reader_create(const char* url);
	void rtmp_reader_destroy(void* rtmp);

	int rtmp_reader_read(void* rtmp, void* packet, int bytes);

	/// set timeout before rtmp_reader_read
	/// @param[in] timeout connect/recv timeout(seconds), default 30sec
	void rtmp_reader_settimeout(void* rtmp, int timeout);

#if defined(__cplusplus)
}
#endif
#endif /* !* _rtmp_reader_h_ */
