#ifndef _rtsp_header_session_h_
#define _rtsp_header_session_h_

struct rtsp_header_session_t
{
	char session[128]; // session id
	int timeout;	   // millisecond
};

int rtsp_header_session(const char* field, struct rtsp_header_session_t* session);

#endif /* !_rtsp_header_session_h_ */
