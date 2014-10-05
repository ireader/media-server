// RFC 2326 Real Time Streaming Protocol (RTSP)
//
// 12.37 Session (p57)
// Session = "Session" ":" session-id [ ";" "timeout" "=" delta-seconds ]
//
#include "rtsp-header-session.h"
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

int rtsp_header_session(const char* field, struct rtsp_header_session_t* session)
{
	const char* p;
	p = strchr(field, ';');
	if(p)
	{
		if(p - field > sizeof(session->session)-1)
		{
			strncpy(session->session, field, sizeof(session->session)-1);
			session->session[sizeof(session->session)-1] = '\0';
		}
		else
		{
			strncpy(session->session, field, p-field);
			session->session[p-field] = '\0';
		}

		if(0 == strncmp("timeout=", p+1, 8))
			session->timeout = (int)(atof(p+9) * 1000);
	}
	else
	{
		strncpy(session->session, field, sizeof(session->session));
		session->session[sizeof(session->session)-1] = '\0';
		session->timeout = 0;
	}

	return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
void rtsp_header_session_test()
{
	struct rtsp_header_session_t session;
	char id1[sizeof(session.session)];
	char id2[sizeof(session.session)+10];

	assert(0 == rtsp_header_session("47112344", &session));
	assert(0 == strcmp("47112344", session.session) && 0 == session.timeout);

	assert(0 == rtsp_header_session("47112344;timeout=10.1", &session));
	assert(0 == strcmp("47112344", session.session) && 10100 == session.timeout);

	memset(id1, '1', sizeof(id1));
	memset(id2, '1', sizeof(id2));
	id1[sizeof(session.session)-1] = '\0';
	assert(0 == rtsp_header_session(id2, &session));
	assert(0 == strcmp(id1, session.session) && 0 == session.timeout);
}
#endif
