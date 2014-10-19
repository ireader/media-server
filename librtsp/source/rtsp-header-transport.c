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

#include "rtsp-header-transport.h"
#include "cstringext.h"
#include "string-util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TRANSPORT_SPECIAL ",;\r\n"

static int Hex2Int(const unsigned char* hex, int len)
{
	int i;
	int v = 0;
	assert(len <= 8);
	for(i = 0; i < len; i++)
	{
		if('a' <= hex[i] && hex[i] <= 'f')
		{
			v = (v << 4) | (hex[i] - 'a');
		}
		else if('A' <= hex[i] && hex[i] <= 'F')
		{
			v = (v << 4) | (hex[i] - 'a');
		}
		else if('0' <= hex[i] && hex[i] <= '9')
		{
			v = (v << 4) | (hex[i] - '0');
		}
	}

	return v;
}

int rtsp_header_transport(const char* field, struct rtsp_header_transport_t* t)
{
	const char* p1;
	const char* p = field;
	size_t n;

	t->multicast = 0; // default unicast
	t->transport = RTSP_TRANSPORT_RTP;
	t->lower_transport = RTSP_TRANSPORT_UDP;

	while(p && *p)
	{
		p1 = string_token(p, TRANSPORT_SPECIAL);
		n = p1 - p;

		if(11 == n && 0 == strnicmp("RTP/AVP/UDP", p, 11))
		{
			t->transport = RTSP_TRANSPORT_RTP;
			t->lower_transport = RTSP_TRANSPORT_UDP;
		}
		else if(11 == n && 0 == strnicmp("RTP/AVP/TCP", p, 11))
		{
			t->transport = RTSP_TRANSPORT_RTP;
			t->lower_transport = RTSP_TRANSPORT_TCP;
		}
		else if(11 == n && 0 == strnicmp("RAW/RAW/UDP", p, 11))
		{
			t->transport = RTSP_TRANSPORT_RAW;
			t->lower_transport = RTSP_TRANSPORT_UDP;
		}
		else if(7 == n && 0 == strnicmp("RTP/AVP", p, 7))
		{
			t->transport = RTSP_TRANSPORT_RTP;
			t->lower_transport = RTSP_TRANSPORT_UDP;
		}
		else if(7 == n && 0 == strnicmp("unicast", p, 7))
		{
			t->multicast = 0;
		}
		else if(9 == n && 0 == strnicmp("multicast", p, 9))
		{
			t->multicast = 1;
		}
		else if(n >= 12 && 0 == strnicmp("destination=", p, 12))
		{
			assert(p1 - p < sizeof(t->destination)-1);
			strncpy(t->destination, p+12, p1-p-12);
			t->destination[p1-p-12] = '\0';
		}
	   else if(n >= 7 && 0 == strnicmp("source=", p, 7))
	   {
		   assert(p1 - p < sizeof(t->source)-1);
		   strncpy(t->source, p+7, p1-p-7);
		   t->source[p1-p-7] = '\0';
	   }
	   else if(13 == n && 0 == strnicmp("ssrc=", p, 5))
	   {
		   // unicast only
		   assert(0 == t->multicast);
		   t->ssrc = Hex2Int((const unsigned char*)p+5, 8);
	   }
	   else if(n > 5 && 0 == strnicmp("mode=", p, 5))
	   {
		   if( (11==n && 0 == stricmp("\"PLAY\"", p+5)) || (9==n && 0 == stricmp("PLAY", p+5)) )
			   t->mode = RTSP_TRANSPORT_PLAY;
		   else if( (13==n && 0 == stricmp("\"RECORD\"", p+5)) || (11==n && 0 == stricmp("RECORD", p+5)) )
			   t->mode = RTSP_TRANSPORT_RECORD;
		   else
			   t->mode = RTSP_TRANSPORT_UNKNOWN;
	   }
	   else if(6 == n && 0 == stricmp("append", p))
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
		   t->port2 = t->port1 + 1;
	   }
	   else if(2 == sscanf(p, "client_port=%hu-%hu", &t->client_port1, &t->client_port2))
	   {
		   assert(0 == t->multicast);
	   }
	   else if(1 == sscanf(p, "client_port=%hu", &t->client_port1))
	   {
		   assert(0 == t->multicast);
		   t->client_port2 = t->client_port1 + 1;
	   }
	   else if(2 == sscanf(p, "server_port=%hu-%hu", &t->server_port1, &t->server_port2))
	   {
		   assert(0 == t->multicast);
	   }
	   else if(1 == sscanf(p, "server_port=%hu", &t->server_port1))
	   {
		   assert(0 == t->multicast);
		   t->server_port2 = t->server_port1 + 1;
	   }
	   else if(2 == sscanf(p, "interleaved=%d-%d", &t->interleaved1, &t->interleaved2))
	   {
	   }
	   else if(1 == sscanf(p, "interleaved=%d", &t->interleaved1))
	   {
		   t->interleaved2 = t->interleaved1 + 1;
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

	   if('\r' == *p1 || '\n' == *p1 || '\0' == *p1)
		   break;
	   p = p1 + 1;
   }

   return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
void rtsp_header_transport_test()
{
	struct rtsp_header_transport_t t;

	memset(&t, 0, sizeof(t));
	assert(0 == rtsp_header_transport("RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257", &t)); // rfc2326 p61
	assert(t.transport==RTSP_TRANSPORT_RTP && t.lower_transport==RTSP_TRANSPORT_UDP);
	assert(t.multicast==0 && t.client_port1==4588 && t.client_port2==4589 && t.server_port1==6256 && t.server_port2==6257);
}
#endif
