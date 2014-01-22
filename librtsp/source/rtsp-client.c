#include "rtsp-client.h"
#include "rtsp-parser.h"
#include "cstringext.h"
#include "sys/sock.h"
#include "aio-socket.h"
#include "url.h"
#include "sdp.h"
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#define USER_AGENT "Netposa RTSP Lib"
#define RTP_PORT_BASE 30000
#define N_MEDIA 2

struct rtsp_media
{
	char ip[64];
	unsigned short port;

	char *uri; // rtsp setup

	socket_t rtp[2]; // rtp connection
	int client_port[2];
	int server_port[2];

	char* session; // rtsp setup session, NULL if Aggregate Control Available
};

struct rtsp_context
{
	unsigned int cseq; // rtsp sequence
	socket_t sock; // rtsp connection

	int media_count;
	struct rtsp_media media[N_MEDIA];
	struct rtsp_media *media_ptr;

	char* session; // rtsp setup session, valid if Aggregate Control Available

	char req[1024];
	char *ptr;
	int bytes;

	char *uri;
	char ip[64];
	unsigned short port;

	void* parser;
	void *sdp; // sdp parser
};

static int rtsp_connect(struct rtsp_context* ctx)
{
	int r;
	if(socket_invalid == ctx->sock || 0!=socket_readable(ctx->sock))
	{
		socket_close(ctx->sock);
		ctx->sock = socket_invalid;
	}

	if(socket_invalid == ctx->sock)
	{
		ctx->sock = socket_tcp();
		r = socket_connect_ipv4_by_time(ctx->sock, ctx->ip, ctx->port, 5000);
		if(0 != r)
		{
			socket_close(ctx->sock);
			ctx->sock = socket_invalid;
			return r;
		}

		socket_setnonblock(ctx->sock, 0); // reset to block mode
	}

	return 0;
}

static int rtsp_disconnect(struct rtsp_context* ctx)
{
	if(socket_invalid != ctx->sock)
	{
		socket_close(ctx->sock);
		ctx->sock = socket_invalid;
	}
	return 0;
}

static int rtsp_send(struct rtsp_context* ctx, const void* p, int n)
{
	int r;
	r = socket_send(ctx->sock, p, n, 0);
	if(r != n)
		return -1;

	return 0;
}

static int rtsp_send_v(struct rtsp_context* ctx, const socket_bufvec_t* v, int n)
{
	int r;
	r = socket_send_v(ctx->sock, v, n, 0);
	if(r != n)
		return -1;

	return 0;
}

static int rtsp_recv(struct rtsp_context* ctx)
{
	int r, s;
	char p[1024];

	rtsp_parser_clear(ctx->parser);

	s = 1;
	do
	{
		r = socket_recv(ctx->sock, p, sizeof(p), 0);
		if(r <= 0)
			break;

		s = rtsp_parser_input(ctx->parser, p, &r);
	} while(0 != s);

	if(0 == s)
	{
		return 200 == rtsp_get_status_code(ctx->parser) ? 0 : -1;
	}
	return -1;
}

static int rtsp_request(struct rtsp_context* ctx, const void* p, int n)
{
	int r = rtsp_connect(ctx);
	if(0 != r)
		return r;

	r = rtsp_send(ctx, p, n);
	if(0 != r)
		return r;

	r = rtsp_recv(ctx);
	if(0 != r)
		return r;

	return 0;
}

static int rtsp_request_v(struct rtsp_context* ctx, const socket_bufvec_t *vec, int n)
{
	int r = rtsp_connect(ctx);
	if(0 != r)
		return r;

	r = rtsp_send_v(ctx, vec, n);
	if(0 != r)
		return r;

	r = rtsp_recv(ctx);
	if(0 != r)
		return r;

	return 0;
}

static socket_t rtp_udp_socket(unsigned short port)
{
	socket_t s;
	s = socket_udp();
	if(socket_invalid == s)
		return s;

	if(0 == socket_bind_any(s, port))
		return s;

	socket_close(s);
	return socket_invalid;
}

static int rtsp_create_rtp_socket(socket_t *rtp, socket_t *rtcp, int *port)
{
	unsigned short i;
	socket_t sock[2];
	assert(0 == RTP_PORT_BASE % 2);
	srand((unsigned int)time(NULL));

	do
	{
		i = rand() % 30000;
		i = i/2*2 + RTP_PORT_BASE;

		sock[0] = rtp_udp_socket(i);
		if(socket_invalid == sock[0])
			continue;

		sock[1] = rtp_udp_socket(i + 1);
		if(socket_invalid == sock[1])
		{
			socket_close(sock[0]);
			continue;
		}

		*rtp = sock[0];
		*rtcp = sock[1];
		*port = i;
		return 0;

	} while(socket_invalid!=sock[0] && socket_invalid!=sock[1]);

	return -1;
}

/*
C->S: 
DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0
CSeq: 312
Accept: application/sdp, application/rtsl, application/mheg

S->C: 
RTSP/1.0 200 OK
CSeq: 312
Date: 23 Jan 1997 15:35:06 GMT
Content-Type: application/sdp
Content-Length: 376

v=0
o=mhandley 2890844526 2890842807 IN IP4 126.16.64.4
s=SDP Seminar
i=A Seminar on the session description protocol
u=http://www.cs.ucl.ac.uk/staff/M.Handley/sdp.03.ps
e=mjh@isi.edu (Mark Handley)
c=IN IP4 224.2.17.12/127
t=2873397496 2873404696
a=recvonly
m=audio 3456 RTP/AVP 0
m=video 2232 RTP/AVP 31
m=whiteboard 32416 UDP WB
a=orient:portrait
*/
static int rtsp_describe(struct rtsp_context* ctx)
{
	int r;
	snprintf(ctx->req, sizeof(ctx->req), 
		"DESCRIBE %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Accept: application/sdp\r\n"
		"User-Agent: %s\r\n"
		"\r\n", 
		ctx->uri, ctx->cseq++, USER_AGENT);

	r = rtsp_request(ctx, ctx->req, strlen(ctx->req));
	if(0 == r)
	{
		if(ctx->sdp) sdp_destroy(ctx->sdp);
		ctx->sdp = sdp_parse(rtsp_get_content(ctx->parser));
		if(!ctx->sdp)
			return -1;
	}
	return r;
}

/*
C->S:
SETUP rtsp://example.com/foo/bar/baz.rm RTSP/1.0
CSeq: 302
Transport: RTP/AVP;unicast;client_port=4588-4589

S->C: 
RTSP/1.0 200 OK
CSeq: 302
Date: 23 Jan 1997 15:35:06 GMT
Session: 47112344
Transport: RTP/AVP;unicast; client_port=4588-4589;server_port=6256-6257
*/

enum { 
	RTSP_TRANSPORT_UNKNOWN = 0, 
	RTSP_TRANSPORT_UNICAST = 1, 
	RTSP_TRANSPORT_MULTICAST,
};

// transport
enum {
	RTSP_TRANSPORT_RTP = 1,
	RTSP_TRANSPORT_RAW,
};

// transport lower transport
enum {
	RTSP_TRANSPORT_UDP = 1, 
	RTSP_TRANSPORT_TCP
};

// transport mode
enum {
	RTSP_TRANSPORT_PLAY = 1, 
	RTSP_TRANSPORT_RECORD
};

struct rtsp_header_transport
{
	int transport; // RTP/RAW
	int lower_transport; // TCP/UDP, RTP/AVP default UDP
	int multicast; // unicast/multicast, default multicast
	char* destination;
	char* source;
	int layer; // rtsp setup response only
	int mode; // PLAY/RECORD, default PLAY, rtsp setup response only
	int append; // use with RECORD mode only, rtsp setup response only
	int interleaved1, interleaved2; // rtsp setup response only
	int ttl; // multicast only
	unsigned short port1, port2; // RTP only
	unsigned short client_port1, client_port2; // unicast RTP/RTCP port pair, RTP only
	unsigned short server_port1, server_port2; // unicast RTP/RTCP port pair, RTP only
	char* ssrc; // RTP only
};

static int rtsp_header_parse_transport(const char* fields, struct rtsp_header_transport* t)
{
	char* p;

	p = malloc(strlen(fields));
	if(!p)
		return -1;

	t->transport = RTSP_TRANSPORT_RTP;
	t->lower_transport = RTSP_TRANSPORT_UDP;

	while(1 == sscanf(fields, "%[^;\r\n]", p))
	{
		if(0 == stricmp("RTP/AVP", p))
		{
			t->transport = RTSP_TRANSPORT_RTP;
			t->lower_transport = RTSP_TRANSPORT_UDP;
		}
		else if(0 == stricmp("RTP/AVP/UDP", p))
		{
			t->transport = RTSP_TRANSPORT_RTP;
			t->lower_transport = RTSP_TRANSPORT_TCP;
		}
		else if(0 == stricmp("RTP/AVP/TCP", p))
		{
			t->transport = RTSP_TRANSPORT_RTP;
			t->lower_transport = RTSP_TRANSPORT_TCP;
		}
		else if(0 == stricmp("RAW/RAW/UDP", p))
		{
			t->transport = RTSP_TRANSPORT_RAW;
			t->lower_transport = RTSP_TRANSPORT_UDP;
		}
		else if(0 == stricmp("unicast", p))
		{
			t->multicast = 0;
		}
		else if(0 == stricmp("multicast", p))
		{
			t->multicast = 1;
		}
		else if(0 == strnicmp("destination=", p, 12))
		{
			t->destination = strdup(p+12);
		}
		else if(0 == strnicmp("source=", p, 7))
		{
			t->source = strdup(p+7);
		}
		else if(0 == strnicmp("ssrc=", p, 5))
		{
			// unicast only
			assert(0 == t->multicast);
			t->ssrc = strdup(p+5);
		}
		else if(0 == strnicmp("mode=", p, 5))
		{
			if(0 == stricmp("PLAY", p+5))
				t->mode = RTSP_TRANSPORT_PLAY;
			else if(0 == stricmp("RECORD", p+5))
				t->mode = RTSP_TRANSPORT_RECORD;
			else
				t->mode = RTSP_TRANSPORT_UNKNOWN;
		}
		else if(0 == stricmp("append", p))
		{
			t->append = 1;
		}
		else if(2 == sscanf(p, "port=%hu-%hu", &t->port1, &t->port2))
		{
			assert(1 == t->multicast);
		}
		else if(1 == sscanf(p, "port=%hu", &t->port1))
		{
			assert(1 == t->multicast);
		}
		else if(2 == sscanf(p, "client_port=%hu-%hu", &t->client_port1, &t->client_port2))
		{
			assert(0 == t->multicast);
		}
		else if(1 == sscanf(p, "client_port=%hu", &t->client_port1))
		{
			assert(0 == t->multicast);
		}
		else if(2 == sscanf(p, "server_port=%hu-%hu", &t->server_port1, &t->server_port2))
		{
			assert(0 == t->multicast);
		}
		else if(1 == sscanf(p, "server_port=%hu", &t->server_port1))
		{
			assert(0 == t->multicast);
		}
		else if(2 == sscanf(p, "interleaved=%hu-%hu", &t->interleaved1, &t->interleaved2))
		{
		}
		else if(1 == sscanf(p, "interleaved=%hu", &t->interleaved1))
		{
		}
		else if(1 == sscanf(p, "ttl=%u", &t->ttl))
		{
			assert(1 == t->multicast);
		}
		else if(1 == sscanf(p, "layers=%d", &t->layer))
		{
			assert(1 == t->multicast);
		}
		else
		{
			assert(0); // unknown parameter
		}

		fields += strlen(p);
		while (*fields == ';') ++fields; // skip over separating ';' chars
		if (*fields == '\0' || *fields == '\r' || *fields == '\n') break;
	}

	free(p);
	return 0;
}

static int isAbsoluteURL(char const* url) 
{
	// Assumption: "url" is absolute if it contains a ':', before any
	// occurrence of '/'
	while (*url != '\0' && *url != '/') {
		if (*url == ':') return 1;
		++url;
	}

	return 0;
}

static int rtsp_setup(struct rtsp_context* ctx, const char* sdp)
{
	int i, r;
	const char *s_control;
	const char *m_control;
	const char *session;
	const char *transport;
	struct rtsp_media* media;
	struct rtsp_header_transport ht;

	if(sdp)
	{
		if(ctx->sdp) sdp_destroy(ctx->sdp);
		ctx->sdp = sdp_parse(sdp);
	}
	if(!ctx->sdp)
	{
		return -1;
	}

	r = sdp_media_count(ctx->sdp);
	if(r > N_MEDIA)
	{
		ctx->media_ptr = (struct rtsp_media*)malloc(sizeof(struct rtsp_media)*(r-N_MEDIA));
		if(!ctx->media_ptr)
			return -1;
		memset(ctx->media_ptr, 0, sizeof(struct rtsp_media)*(r-N_MEDIA));
	}

	ctx->media_count = r;
	for(i = 0; i < ctx->media_count; i++)
	{
		if(i < N_MEDIA)
			media = ctx->media + i;
		else
			media = ctx->media_ptr + i - N_MEDIA;

		if(0 == sdp_media_get_connection_address(ctx->sdp, i, media->ip, sizeof(media->ip)-1))
		{
			assert(sdp_media_get_connection_network(ctx->sdp, i)==SDP_C_NETWORK_IN);
			switch(sdp_media_get_connection_addrtype(ctx->sdp, i))
			{
			case SDP_C_ADDRESS_IP4:
				media->port = 554;
				sdp_media_port(ctx->sdp, i, &media->port);
				break;

			case SDP_C_ADDRESS_IP6:
				assert(0);
				break;

			default:
				assert(0);
			}
		}

		if(0 != rtsp_create_rtp_socket(&media->rtp[0], &media->rtp[1], &media->client_port[0]))
		{
			printf("rtsp_create_rtp_socket error.\n");
			return -1;
		}
		media->client_port[1] = media->client_port[0] + 1;

		// RTSP2326 C.1.1 Control URL
		m_control = sdp_media_attribute_find(ctx->sdp, i, "control");
		if(isAbsoluteURL(m_control))
		{
			media->uri = strdup(m_control);
		}
		else
		{
			s_control = sdp_attribute_find(ctx->sdp, i, "control");
			if(!s_control || 0 == s_control[0] || 0 == strcmp('*', s_control))
			{
				s_control = rtsp_get_header_by_name(ctx->parser, "Content-Base");
				if(!s_control)
				{
					s_control = rtsp_get_header_by_name(ctx->parser, "Content-Location");
					if(!s_control)
						s_control = ctx->uri;
				}
			}

			r = strlen(s_control);
			media->uri = (char*)malloc(r+1+strlen(m_control)+1);
			if(!media->uri)
			{
				return -1;
			}
			sprintf(media->uri, "%s%s%s", s_control, ('/'==s_control[r-1]||'/'==m_control[0])?"":"/", m_control);
		}

		// 224.0.0.0 ~ 239.255.255.255
		if(ctx->session)
		{
			snprintf(ctx->req, sizeof(ctx->req), 
				"SETUP %s RTSP/1.0\r\n"
				"CSeq: %u\r\n"
				"Session: %s\r\n"
				"Transport: RTP/AVP;unicast;client_port=%u-%u\r\n"
				"User-Agent: %s\r\n"
				"\r\n", 
				media->uri, ctx->cseq++, ctx->session, media->client_port[0], media->client_port[1], USER_AGENT);
		}
		else
		{
			snprintf(ctx->req, sizeof(ctx->req), 
				"SETUP %s RTSP/1.0\r\n"
				"CSeq: %u\r\n"
				"Transport: RTP/AVP;unicast;client_port=%u-%u\r\n"
				"User-Agent: %s\r\n"
				"\r\n", 
				media->uri, ctx->cseq++, media->client_port[0], media->client_port[1], USER_AGENT);
		}

		r = rtsp_request(ctx, ctx->req, strlen(ctx->req));
		if(0 == r)
		{
			session = rtsp_get_header_by_name(ctx->parser, "Session");
			if(!session)
			{
				return -1;
			}

			transport = rtsp_get_header_by_name(ctx->parser, "Transport");
			if(!transport)
			{
				return -1;
			}

			memset(&ht, 0, sizeof(ht));
			if(0 != rtsp_header_parse_transport(transport, &ht))
			{
				return -1;
			}

			assert(0 == ht.multicast);
			if(0 != ht.server_port1)
			{
				media->server_port[0] = ht.server_port1;
				if(0 == ht.server_port2)
					media->server_port[1] = ht.server_port2;
				else
					media->server_port[1] = ht.server_port1+1;
			}

			if(ht.ssrc)
				free(ht.ssrc);
			if(ht.source)
				free(ht.source);
			if(ht.destination)
				free(ht.destination);

			media->session = strdup(session);
			if(!ctx->session)
				ctx->session = strdup(session);
		}
	}
	return r;
}

/*
ANNOUNCE rtsp://server.example.com/fizzle/foo RTSP/1.0
CSeq: 312
Date: 23 Jan 1997 15:35:06 GMT
Session: 47112344
Content-Type: application/sdp
Content-Length: 332

v=0
o=mhandley 2890844526 2890845468 IN IP4 126.16.64.4
s=SDP Seminar
i=A Seminar on the session description protocol
u=http://www.cs.ucl.ac.uk/staff/M.Handley/sdp.03.ps
e=mjh@isi.edu (Mark Handley)
c=IN IP4 224.2.17.12/127
t=2873397496 2873404696
a=recvonly
m=audio 3456 RTP/AVP 0
m=video 2232 RTP/AVP 31
*/
static int rtsp_announce(struct rtsp_context* ctx, const char* sdp)
{
	socket_bufvec_t vec[2];

	snprintf(ctx->req, sizeof(ctx->req), 
		"ANNOUNCE %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %s\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: %d\r\n"
		"\r\n", 
		ctx->uri, ctx->cseq++, ctx->session, strlen(sdp));

	socket_setbufvec(vec, 0, ctx->req, strlen(ctx->req));
	socket_setbufvec(vec, 1, (void*)sdp, strlen(sdp));
	return rtsp_request_v(ctx, vec, 2);
}

/*
C->S: 
TEARDOWN rtsp://example.com/fizzle/foo RTSP/1.0
CSeq: 892
Session: 12345678

S->C: 
RTSP/1.0 200 OK
CSeq: 892
*/
static int rtsp_teardown(struct rtsp_context* ctx)
{
	int r;
	snprintf(ctx->req, sizeof(ctx->req), 
		"TEARDOWN %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %s\r\n"
		"\r\n", 
		ctx->uri, ctx->cseq++, ctx->session);

	r = rtsp_request(ctx, ctx->req, strlen(ctx->req));
	if(0 == r)
	{
		assert(ctx->session);
		if(ctx->session)
			free(ctx->session);
	}
	return r;
}

void* rtsp_open(const char* uri)
{
	int r;
	void *parser;
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)malloc(sizeof(struct rtsp_context));
	if(!ctx)
		return NULL;

	socket_init();
	parser = url_parse(uri);
	if(!parser)
	{
		free(ctx);
		return NULL;
	}

	memset(ctx, 0, sizeof(struct rtsp_context));
	strncpy(ctx->ip, url_gethost(parser), sizeof(ctx->ip)-1);
	ctx->port = url_getport(parser);
	if(0 == ctx->port)
		ctx->port = 554; // default

	srand(time(NULL));
	ctx->cseq = rand();
	ctx->sock = socket_invalid;
	ctx->parser = rtsp_parser_create(RTSP_PARSER_CLIENT);
	ctx->uri = strdup(uri);
	r = rtsp_describe(ctx);
	r = rtsp_setup(ctx, NULL);
	return ctx;
}

int rtsp_close(void* rtsp)
{
	int r;
	struct rtsp_context *ctx;

	ctx = (struct rtsp_context*)rtsp;
	r = rtsp_teardown(ctx);

	if(ctx->sock)
		socket_close(ctx->sock);
	return 0;
}

/*
PLAY rtsp://audio.example.com/audio RTSP/1.0
CSeq: 835
Session: 12345678
Range: npt=10-15

C->S: 
PLAY rtsp://audio.example.com/twister.en RTSP/1.0
CSeq: 833
Session: 12345678
Range: smpte=0:10:20-;time=19970123T153600Z

S->C: 
RTSP/1.0 200 OK
CSeq: 833
Date: 23 Jan 1997 15:35:06 GMT
Range: smpte=0:10:22-;time=19970123T153600Z

C->S: 
PLAY rtsp://audio.example.com/meeting.en RTSP/1.0
CSeq: 835
Session: 12345678
Range: clock=19961108T142300Z-19961108T143520Z

S->C: 
RTSP/1.0 200 OK
CSeq: 835
Date: 23 Jan 1997 15:35:06 GMT
*/
int rtsp_play(void* rtsp)
{
	int r;
	struct rtsp_context *ctx;

	ctx = (struct rtsp_context*)rtsp;
	snprintf(ctx->req, sizeof(ctx->req), 
		"PLAY %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %s\r\n"
		"Range: npt=-\r\n"
		"\r\n", 
		ctx->uri, ctx->cseq++, ctx->session);

	r = rtsp_request(ctx, ctx->req, strlen(ctx->req));
	return r;
}

/*
C->S: 
PAUSE rtsp://example.com/fizzle/foo RTSP/1.0
CSeq: 834
Session: 12345678

S->C: 
RTSP/1.0 200 OK
CSeq: 834
Date: 23 Jan 1997 15:35:06 GMT
*/
// A PAUSE request discards all queued PLAY requests. However, the pause
// point in the media stream MUST be maintained. A subsequent PLAY
// request without Range header resumes from the pause point.
int rtsp_pause(void* rtsp)
{
	int r;
	struct rtsp_context *ctx;

	ctx = (struct rtsp_context*)rtsp;
	snprintf(ctx->req, sizeof(ctx->req), 
		"PAUSE %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %s\r\n"
		"\r\n", 
		ctx->uri, ctx->cseq++, ctx->session);

	r = rtsp_request(ctx, ctx->req, strlen(ctx->req));
	return r;
}
