#include "rtsp-header-transport.h"
#include "cstringext.h"
#include <stdlib.h>
#include <string.h>

// RFC-2326 RTSP 12.39 Transport
// Transport: RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257
int rtsp_header_transport_parse(const char* fields, struct rtsp_header_transport* t)
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
