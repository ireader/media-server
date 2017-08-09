// RFC 2326 Real Time Streaming Protocol (RTSP)
// 12.39 Transport (p58)
//
// Transport = "Transport" ":" 1#transport-spec
// transport-spec = transport-protocol/profile[/lower-transport] *parameter
// transport-protocol = "RTP"
// profile = "AVP"
// lower-transport = "TCP" | "UDP"
// parameter = ( "unicast" | "multicast" )
//				| ";" "destination" [ "=" address ]
//				| ";" "interleaved" "=" channel [ "-" channel ]
//				| ";" "append"
//				| ";" "ttl" "=" ttl
//				| ";" "layers" "=" 1*DIGIT
//				| ";" "port" "=" port [ "-" port ]
//				| ";" "client_port" "=" port [ "-" port ]
//				| ";" "server_port" "=" port [ "-" port ]
//				| ";" "ssrc" "=" ssrc
//				| ";" "mode" = <"> 1\#mode <">
// ttl = 1*3(DIGIT)
// port = 1*5(DIGIT)
// ssrc = 8*8(HEX)
// channel = 1*3(DIGIT)
// address = host
// mode = <"> *Method <"> | Method
//
// Transport: RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257
// Transport: RTP/AVP;multicast;ttl=127;mode="PLAY",RTP/AVP;unicast;client_port=3456-3457;mode="PLAY"

// RTP Port define
// RFC 3550: 11. RTP over Network and Transport Protocols (p56)
// 1. For UDP and similar protocols, RTP should use an even destination port number and
//    the corresponding RTCP stream should use the next higher (odd) destination port number. 
// 2. For applications that take a single port number as a parameter and derive the RTP and RTCP port
//    pair from that number, if an odd number is supplied then the application should replace that
//    number with the next lower (even) number to use as the base of the port pair.

#include "rtsp-header-transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64) || defined(OS_WINDOWS)
#define strcasecmp _stricmp
#define strncasecmp	_strnicmp
#endif

#define TRANSPORT_SPECIAL ",;\r\n"

int rtsp_header_transport(const char* field, struct rtsp_header_transport_t* t)
{
	const char* p1;
	const char* p = field;
    size_t n;

	memset(t, 0, sizeof(*t));
	t->multicast = 0; // default unicast
	t->transport = RTSP_TRANSPORT_RTP_UDP;

	while(p && *p)
	{
		p1 = strpbrk(p, TRANSPORT_SPECIAL);
		n = p1 ? (size_t)(p1 - p) : strlen(p); // ptrdiff_t -> size_t

		switch(*p)
		{
		case 'r':
		case 'R':
			if(11 == n && 0 == strncasecmp("RTP/AVP/UDP", p, 11))
			{
				t->transport = RTSP_TRANSPORT_RTP_UDP;
			}
			else if(11 == n && 0 == strncasecmp("RTP/AVP/TCP", p, 11))
			{
				t->transport = RTSP_TRANSPORT_RTP_TCP;
			}
			else if(11 == n && 0 == strncasecmp("RAW/RAW/UDP", p, 11))
			{
				t->transport = RTSP_TRANSPORT_RAW;
			}
			else if(7 == n && 0 == strncasecmp("RTP/AVP", p, 7))
			{
				t->transport = RTSP_TRANSPORT_RTP_UDP;
			}
			break;

		case 'u':
		case 'U':
			if(7 == n && 0 == strncasecmp("unicast", p, 7))
			{
				t->multicast = 0;
			}
			break;

		case 'm':
		case 'M':
			if(9 == n && 0 == strncasecmp("multicast", p, 9))
			{
				t->multicast = 1;
			}
			else if(n > 5 && 0 == strncasecmp("mode=", p, 5))
			{
				if( (11==n && 0 == strcasecmp("\"PLAY\"", p+5)) || (9==n && 0 == strcasecmp("PLAY", p+5)) )
					t->mode = RTSP_TRANSPORT_PLAY;
				else if( (13==n && 0 == strcasecmp("\"RECORD\"", p+5)) || (11==n && 0 == strcasecmp("RECORD", p+5)) )
					t->mode = RTSP_TRANSPORT_RECORD;
			}
			break;

		case 'd':
		case 'D':
			if(n >= 12 && 0 == strncasecmp("destination=", p, 12))
			{
                if(n-12 >= sizeof(t->destination)) return -1;
                memcpy(t->destination, p+12, n - 12);
                t->destination[n-12] = '\0';
			}
			break;

		case 's':
		case 'S':
			if(n >= 7 && 0 == strncasecmp("source=", p, 7))
			{
                if(n-7 >= sizeof(t->source)) return -1;
                memcpy(t->source, p+7, n - 7);
                t->source[n-7] = '\0';
			}
			else if(13 == n && 0 == strncasecmp("ssrc=", p, 5))
			{
				// unicast only
				assert(0 == t->multicast);
				t->rtp.u.ssrc = (int)strtol(p+5, NULL, 16);
			}
			else if(2 == sscanf(p, "server_port=%hu-%hu", &t->rtp.u.server_port1, &t->rtp.u.server_port2))
			{
				assert(0 == t->multicast);
			}
			else if(1 == sscanf(p, "server_port=%hu", &t->rtp.u.server_port1))
			{
				assert(0 == t->multicast);
				t->rtp.u.server_port1 = t->rtp.u.server_port1 / 2 * 2; // RFC 3550 (p56)
				t->rtp.u.server_port2 = t->rtp.u.server_port1 + 1;
			}
			break;

		case 'a':
			if(6 == n && 0 == strcasecmp("append", p))
			{
				t->append = 1;
			}
			break;

		case 'p':
			if(2 == sscanf(p, "port=%hu-%hu", &t->rtp.m.port1, &t->rtp.m.port2))
			{
				assert(1 == t->multicast);
			}
			else if(1 == sscanf(p, "port=%hu", &t->rtp.m.port1))
			{
				assert(1 == t->multicast);
				t->rtp.m.port1 = t->rtp.m.port1 / 2 * 2; // RFC 3550 (p56)
				t->rtp.m.port2 = t->rtp.m.port1 + 1;
			}
			break;

		case 'c':
			if(2 == sscanf(p, "client_port=%hu-%hu", &t->rtp.u.client_port1, &t->rtp.u.client_port2))
			{
				assert(0 == t->multicast);
			}
			else if(1 == sscanf(p, "client_port=%hu", &t->rtp.u.client_port1))
			{
				assert(0 == t->multicast);
				t->rtp.u.client_port1 = t->rtp.u.client_port1 / 2 * 2; // RFC 3550 (p56)
				t->rtp.u.client_port2 = t->rtp.u.client_port1 + 1;
			}
			break;

		case 'i':
			if(2 == sscanf(p, "interleaved=%d-%d", &t->interleaved1, &t->interleaved2))
			{
			}
			else if(1 == sscanf(p, "interleaved=%d", &t->interleaved1))
			{
				t->interleaved2 = t->interleaved1 + 1;
			}
			break;

		case 't':
			if(1 == sscanf(p, "ttl=%d", &t->rtp.m.ttl))
			{
				assert(1 == t->multicast);
			}
			break;

		case 'l':
			if(1 == sscanf(p, "layers=%d", &t->layer))
			{
			}
			break;
		}

	   if(NULL == p1 || '\r' == *p1 || '\n' == *p1 || '\0' == *p1 || ',' == *p1)
		   break;
	   p = p1 + 1;
   }

   return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
void rtsp_header_transport_test(void)
{
	struct rtsp_header_transport_t t;

	memset(&t, 0, sizeof(t));
	assert(0 == rtsp_header_transport("RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257;ssrc=08abe80f", &t)); // rfc2326 p61
	assert(t.transport==RTSP_TRANSPORT_RTP_UDP);
	assert(t.multicast==0 && t.rtp.u.client_port1==4588 && t.rtp.u.client_port2==4589 && t.rtp.u.server_port1==6256 && t.rtp.u.server_port2==6257);
	assert(t.rtp.u.ssrc == 0x08abe80f);

	memset(&t, 0, sizeof(t));
	assert(0 == rtsp_header_transport("RTP/AVP;multicast;ttl=127;mode=\"PLAY\"", &t)); // rfc2326 p61
	assert(t.transport==RTSP_TRANSPORT_RTP_UDP);
	assert(t.multicast==1 && 127==t.rtp.m.ttl && RTSP_TRANSPORT_PLAY==t.mode);

	memset(&t, 0, sizeof(t));
	assert(0 == rtsp_header_transport("RTP/AVP/TCP;interleaved=0-1", &t)); // rfc2326 p40
	assert(t.transport == RTSP_TRANSPORT_RTP_TCP);
	assert(t.interleaved1 == 0 && t.interleaved2 == 1);

	memset(&t, 0, sizeof(t));
	assert(0 == rtsp_header_transport("RTP/AVP;unicast;source=192.168.111.333.444.555.666.777.888.999.000.123", &t)); // rfc2326 p61
	assert(t.transport==RTSP_TRANSPORT_RTP_UDP);
	assert(t.multicast==0 && 0==strncmp("192.168.111.333.444.555.666.777.888.999.000.123", t.source, sizeof(t.source)-1) && strlen(t.source)<sizeof(t.source));
}
#endif
