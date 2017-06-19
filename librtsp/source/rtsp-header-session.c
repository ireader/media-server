// RFC 2326 Real Time Streaming Protocol (RTSP)
//
// 12.37 Session (p57)
// Session = "Session" ":" session-id [ ";" "timeout" "=" delta-seconds ]
//
#include "rtsp-header-session.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int rtsp_header_session(const char* field, struct rtsp_header_session_t* session)
{
	const char* p;

	// RFC2326 12.37 Session (p57)
	// The timeout is measured in seconds, with a default of 60 seconds (1 minute).
    session->timeout = 60000;

    p = strchr(field, ';');
	if(p)
	{
        size_t n = (size_t)(p - field);
		if(n >= sizeof(session->session))
            return -1;

		memcpy(session->session, field, n);
        session->session[n] = '\0';

		if(0 == strncmp("timeout=", p+1, 8))
			session->timeout = (int)(atof(p+9) * 1000);
	}
	else
	{
#if defined(OS_MAC)
		strlcpy(session->session, field, sizeof(session->session));
#else
		size_t n = strlen(field);
		if(n >= sizeof(session->session))
			return -1;
		memcpy(session->session, field, n);
		session->session[n] = '\0';
#endif
	}

	return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
void rtsp_header_session_test(void)
{
	struct rtsp_header_session_t session;
	char id1[sizeof(session.session)];
	char id2[sizeof(session.session)+10];

	assert(0 == rtsp_header_session("47112344", &session));
	assert(0 == strcmp("47112344", session.session) && 60000 == session.timeout);

	assert(0 == rtsp_header_session("47112344;timeout=10.1", &session));
	assert(0 == strcmp("47112344", session.session) && 10100 == session.timeout);

	memset(id1, '1', sizeof(id1));
	memset(id2, '1', sizeof(id2));
	id1[sizeof(session.session)-1] = '\0';
	assert(0 != rtsp_header_session(id2, &session));
}
#endif
