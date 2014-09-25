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
#include "rtsp-util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int rtsp_header_transport(const char* fields, struct rtsp_header_transport_t* t)
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
		   t->lower_transport = RTSP_TRANSPORT_UDP;
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
		   if(0 == stricmp("\"PLAY\"", p+5) || 0 == stricmp("PLAY", p+5))
			   t->mode = RTSP_TRANSPORT_PLAY;
		   else if(0 == stricmp("\"RECORD\"", p+5) || 0 == stricmp("RECORD", p+5))
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

#if defined(DEBUG) || defined(_DEBUG)
void rtsp_header_transport_test()
{
	struct rtsp_header_transport_t t;

	memset(&t, 0, sizeof(t));
	assert(0 == rtsp_header_transport("RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257", &t)); // rfc2326 p61
	assert(t.transport==RTSP_TRANSPORT_RTP && t.lower_transport==RTSP_TRANSPORT_UDP);
	assert(t.multicast==0 && t.client_port1==4588 && t.client_port2==4589 && t.server_port1==6255 && t.server_port2==6257);
}
#endif
