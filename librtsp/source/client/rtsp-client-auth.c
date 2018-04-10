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
#include "base64.h"
#include "md5.h"

static void binary_to_hex(char* str, const uint8_t* data, int bytes)
{
	int i;
	const char hex[] = "0123456789abcdef";

	for (i = 0; i < bytes; i++)
	{
		str[i * 2] = hex[data[i] >> 4];
		str[i * 2 + 1] = hex[data[i] & 0xF];
	}

	str[bytes * 2] = 0;
}

static void	md5_A1(char A1[33], const char* algorithm, const char* usr, const char* pwd, const char* realm, const char* nonce, const char* cnonce)
{
	MD5_CTX ctx;
	unsigned char md5[16];

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)usr, strlen(usr));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)realm, strlen(realm));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)pwd, strlen(pwd));
	MD5Final(md5, &ctx);

	// algorithm endwith -sess
	if (0 == strncmp(algorithm + strlen(algorithm) - 5, "-sess", 5))
	{
		MD5Init(&ctx);
		MD5Update(&ctx, md5, 16);
		MD5Update(&ctx, (unsigned char*)":", 1);
		MD5Update(&ctx, (unsigned char*)nonce, strlen(nonce));
		MD5Update(&ctx, (unsigned char*)":", 1);
		MD5Update(&ctx, (unsigned char*)cnonce, strlen(cnonce));
		MD5Final(md5, &ctx);
	}

	binary_to_hex(A1, md5, 16);
}

static void	md5_A2(char A2[33], const char* method, const char* uri, const char* qop, const char* md5entity)
{
	MD5_CTX ctx;
	unsigned char md5[16];

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)method, strlen(method));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)uri, strlen(uri));
	if (0 == strcasecmp("auth-int", qop))
	{
		MD5Update(&ctx, (unsigned char*)":", 1);
		MD5Update(&ctx, (unsigned char*)md5entity, 32);
	}
	MD5Final(md5, &ctx);

	binary_to_hex(A2, md5, 16);
}

static void md5_response(char reponse[33], const char* A1, const char* A2, const char* nonce, const char* nc, const char* cnonce, const char* qop)
{
	MD5_CTX ctx;
	unsigned char md5[16];

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)A1, 32);
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)nonce, strlen(nonce));
	MD5Update(&ctx, (unsigned char*)":", 1);
	if (*qop)
	{
		MD5Update(&ctx, (unsigned char*)nc, strlen(nc));
		MD5Update(&ctx, (unsigned char*)":", 1);
		MD5Update(&ctx, (unsigned char*)cnonce, strlen(cnonce));
		MD5Update(&ctx, (unsigned char*)":", 1);
		MD5Update(&ctx, (unsigned char*)qop, strlen(qop));
		MD5Update(&ctx, (unsigned char*)":", 1);
	}
	MD5Update(&ctx, (unsigned char*)A2, 32);
	MD5Final(md5, &ctx);

	binary_to_hex(reponse, md5, 16);
}

static void	md5_username(char r[33], const char* user, const char* realm)
{
	MD5_CTX ctx;
	unsigned char md5[16];

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)user, strlen(user));
	MD5Update(&ctx, (unsigned char*)":", 1);
	MD5Update(&ctx, (unsigned char*)realm, strlen(realm));
	MD5Final(md5, &ctx);

	binary_to_hex(r, md5, 16);
}

static void	md5_entity(char r[33], const uint8_t* entity, int bytes)
{
	MD5_CTX ctx;
	unsigned char md5[16];

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)entity, bytes);
	MD5Final(md5, &ctx);

	binary_to_hex(r, md5, 16);
}

int rtsp_client_authenrization(struct rtsp_client_t* rtsp, const char* method, const char* uri, const char* content, int length, char* authenrization, int bytes)
{
	int n;
	char buffer[1024];

	if (HTTP_AUTHENTICATION_BASIC == rtsp->auth.scheme)
	{
		n = snprintf(buffer, sizeof(buffer), "%s:%s", rtsp->usr, rtsp->pwd);
		if (n < 0 || n + 1 > sizeof(buffer) || (n + 2) / 3 * 4 + n / 57 + 1 > bytes)
			return -E2BIG;

		strcpy(authenrization, "Authorization: Basic ");
		n = base64_encode(authenrization + 21, buffer, n);
		assert(n + 24 < bytes);
		authenrization[21 + n] = '\r';
		authenrization[22 + n] = '\n';
		authenrization[23 + n] = 0;
		return n;
	}
	else if (HTTP_AUTHENTICATION_DIGEST == rtsp->auth.scheme)
	{
		char A1[33];
		char A2[33];
		char entity[33];
		char response[33];
		char username[33];

		// username
		if (rtsp->auth.userhash)
			md5_username(username, rtsp->usr, rtsp->auth.realm);

		md5_entity(entity, (const uint8_t*)content, length); // empty entity
		md5_A1(A1, rtsp->auth.algorithm, rtsp->usr, rtsp->pwd, rtsp->auth.realm, rtsp->auth.nonce, rtsp->cnonce);
		md5_A2(A2, method, uri, rtsp->auth.qop, entity);
		md5_response(response, A1, A2, rtsp->auth.nonce, rtsp->nc, rtsp->cnonce, rtsp->auth.qop);

		//n = snprintf(buffer, sizeof(buffer), ", algorithm=MD5");
		n = snprintf(buffer, sizeof(buffer), "%s", ""); // default MD5
		if (n + 1 < sizeof(buffer) && rtsp->auth.opaque[0])
			n += snprintf(buffer + n, sizeof(buffer) - n - 1, ", opaque=\"%s\", ", rtsp->auth.opaque);
		if (n + 1 < sizeof(buffer) && rtsp->auth.qop[0])
			n += snprintf(buffer + n, sizeof(buffer) - n - 1, ", cnonce=\"%s\", nc=%s, qop=%s", rtsp->cnonce, rtsp->nc, rtsp->auth.qop);
		if (n + 1 < sizeof(buffer) && rtsp->auth.userhash)
			n += snprintf(buffer + n, sizeof(buffer) - n - 1, ", userhash=%s", rtsp->auth.userhash ? "true" : "false");

		return snprintf(authenrization, bytes, "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"%s\r\n",
			rtsp->auth.userhash ? username : rtsp->usr,
			rtsp->auth.realm,
			rtsp->auth.nonce,
			uri,
			response,
			buffer);
	}
	else
	{
		// nothing to do
		return 0;
	}
}

int rtsp_client_www_authenticate(struct rtsp_client_t* rtsp, const char* filed)
{
	memset(&rtsp->auth, 0, sizeof(rtsp->auth));
	if (0 != http_header_www_authenticate(filed, &rtsp->auth))
	{
		assert(0);
		return -1;
	}

	if (1 != rtsp->auth.scheme && 2 != rtsp->auth.scheme)
	{
		// only Basic/Digest support
		assert(0);
		return -1;
	}

	if (1 != rtsp->auth.scheme && 0 != strcasecmp(rtsp->auth.algorithm, "MD5") && 0 != rtsp->auth.algorithm[0])
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
	strcpy(rtsp.usr, "Mufasa");
	strcpy(rtsp.pwd, "Circle Of Life");
	strcpy(rtsp.cnonce, "0a4f113b");
	strcpy(rtsp.nc, "00000001");
	rtsp.auth.scheme = HTTP_AUTHENTICATION_DIGEST;
	strcpy(rtsp.auth.realm, "testrealm@host.com");
	strcpy(rtsp.auth.nonce, "dcd98b7102dd2f0e8b11d0f600bfb0c093");
	strcpy(rtsp.auth.opaque, "5ccc069c403ebaf9f0171e9517f40e41");
	strcpy(rtsp.auth.qop, "auth");
	rtsp_client_authenrization(&rtsp, "GET", "/dir/index.html", NULL, 0, rtsp.req, sizeof(rtsp.req));
	assert(strstr(rtsp.req, "response=\"6629fae49393a05397450978507c4ef1\""));
}
#endif
