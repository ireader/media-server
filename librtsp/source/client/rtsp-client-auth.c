/*
S->C:
RTSP/1.0 401 Unauthorized
CSeq: 302
Date: 23 Jan 1997 15:35:06 GMT
WWW-Authenticate: Digest
				realm="http-auth@example.org",
				qop="auth, auth-int",
				algorithm=SHA-256,
				nonce="7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v",
				opaque="FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS"
WWW-Authenticate: Digest
				realm="http-auth@example.org",
				qop="auth, auth-int",
				algorithm=MD5,
				nonce="7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v",
				opaque="FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS"

C->S:
DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0
CSeq: 312
Accept: application/sdp, application/rtsl, application/mheg
Authorization: Digest username="Mufasa",
			realm="http-auth@example.org",
			uri="/dir/index.html",
			algorithm=MD5,
			nonce="7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v",
			nc=00000001,
			cnonce="f2/wE4q74E6zIJEtWaHKaf5wv/H5QzzpXusqGemxURZJ",
			qop=auth,
			response="8ca523f5e9506fed4657c9700eebdbec",
			opaque="FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS"
*/

#include "rtsp-client-internal.h"

int rtsp_client_authenrization(struct rtsp_client_t* rtsp, const char* method, const char* uri, const char* content, int length, char* authenrization, int bytes)
{
	int n;
	if (0 != rtsp->auth.scheme)
	{
		rtsp->auth.nc += 1;
		snprintf(rtsp->auth.uri, sizeof(rtsp->auth.uri), "%s", uri);
		//snprintf(rtsp->auth.cnonce, sizeof(rtsp->auth.cnonce), "%p", rtsp); // TODO

		n = snprintf(authenrization, bytes, "Authorization: ");
		n += http_header_auth(&rtsp->auth, rtsp->pwd, method, content, length, authenrization + n, bytes - n);
		n += snprintf(authenrization + n, bytes - n, "\r\n");
	}
	else
	{
		n = 0;
		rtsp->authenrization[0] = 0;
	}
	return n;
}

int rtsp_client_www_authenticate(struct rtsp_client_t* rtsp, const char* filed)
{
	memset(&rtsp->auth, 0, sizeof(rtsp->auth));
	if (0 != http_header_www_authenticate(filed, &rtsp->auth))
	{
		assert(0);
		return -1;
	}

	if (HTTP_AUTHENTICATION_BASIC != rtsp->auth.scheme && HTTP_AUTHENTICATION_DIGEST != rtsp->auth.scheme)
	{
		// only Basic/Digest support
		assert(0);
		return -1;
	}

	if (HTTP_AUTHENTICATION_BASIC != rtsp->auth.scheme && 0 != strcasecmp(rtsp->auth.algorithm, "MD5") && 0 != rtsp->auth.algorithm[0])
	{
		// only MD5 Digest support
		assert(0);
		return -1;
	}

	// support auth/auth-int only 
	if (0 == memcmp(rtsp->auth.qop, "auth", 4))
	{
		rtsp->auth.qop[4] = 0;
	}
	else
	{
		// compatibility RFC2617
		rtsp->auth.qop[0] = 0;
	}

	return 0;
}

#if defined(_DEBUG) || defined(DEBUG)
void rtsp_client_auth_test()
{
	struct rtsp_client_t rtsp;
	memset(&rtsp, 0, sizeof(rtsp));
	strcpy(rtsp.pwd, "Circle Of Life");
	rtsp.auth.scheme = HTTP_AUTHENTICATION_DIGEST;
	strcpy(rtsp.auth.username, "Mufasa");
	strcpy(rtsp.auth.realm, "testrealm@host.com");
	strcpy(rtsp.auth.nonce, "dcd98b7102dd2f0e8b11d0f600bfb0c093");
	strcpy(rtsp.auth.opaque, "5ccc069c403ebaf9f0171e9517f40e41");
	strcpy(rtsp.auth.cnonce, "0a4f113b");
	strcpy(rtsp.auth.qop, "auth");
	rtsp_client_authenrization(&rtsp, "GET", "/dir/index.html", NULL, 0, rtsp.req, sizeof(rtsp.req));
	assert(strstr(rtsp.req, "response=\"6629fae49393a05397450978507c4ef1\""));
}
#endif
