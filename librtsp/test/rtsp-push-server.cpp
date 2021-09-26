#include "cstringext.h"
#include "sys/sock.h"
#include "sys/system.h"
#include "sys/path.h"
#include "sys/sync.hpp"
#include "aio-worker.h"
#include "ctypedef.h"
#include "ntp-time.h"
#include "sockpair.h"
#include "rtp-profile.h"
#include "rtsp-server.h"
#include "rtsp-media.h"
#include "rtsp-header-transport.h"
#include "rtsp-server-aio.h"
#include "uri-parse.h"
#include "urlcodec.h"
#include "path.h"
#include <map>
#include <list>
#include <memory>
#include "cpm/shared_ptr.h"

extern "C" void rtp_receiver_test(socket_t rtp[2], const char* peer, int peerport[2], int payload, const char* encoding);

struct rtsp_stream_t
{
	std::shared_ptr<rtsp_media_t> media;
	struct rtsp_header_transport_t transport;
	socket_t socket[2]; // rtp/rtcp socket
};

struct rtsp_source_t
{
	int count;
	struct rtsp_media_t media[8];
};

struct rtsp_session_t
{
	std::list<std::shared_ptr<rtsp_stream_t> > streams;
	int status; // setup-init, 1-play, 2-pause
};

typedef std::map<std::string, std::shared_ptr<rtsp_session_t> > TSessions;
static std::map<std::string, std::shared_ptr<rtsp_source_t> > s_sources;
static TSessions s_sessions;
static ThreadLocker s_locker;

static int rtsp_uri_parse(const char* uri, std::string& path)
{
	char path1[256];
	struct uri_t* r = uri_parse(uri, strlen(uri));
	if (!r)
		return -1;

	url_decode(r->path, strlen(r->path), path1, sizeof(path1));
	path = path1;
	uri_free(r);
	return 0;
}

static int rtsp_onannounce(void* /*ptr*/, rtsp_server_t* rtsp, const char* uri, const char* sdp, int len)
{
	std::string filename;
	TSessions::const_iterator it;
	std::shared_ptr<rtsp_source_t> source(new rtsp_source_t);

	rtsp_uri_parse(uri, filename);
	
	source->count = rtsp_media_sdp(sdp, len, source->media, sizeof(source->media)/sizeof(source->media[0]));
	if(source->count < 0 || source->count > sizeof(source->media) / sizeof(source->media[0]))
		return rtsp_server_reply_announce(rtsp, 451); // Parameter Not Understood

	const char* contentBase = rtsp_server_get_header(rtsp, "Content-Base");
	const char* contentLocation = rtsp_server_get_header(rtsp, "Content-Location");
	for (int i = 0; i < source->count; i++)
	{
		if (source->media[i].avformat_count < 1)
		{
			assert(0);
			// 451 Parameter Not Understood
			return rtsp_server_reply_announce(rtsp, 451);
		}

		// rfc 2326 C.1.1 Control URL (p80)
		// If found at the session level, the attribute indicates the URL for aggregate control
		rtsp_media_set_url(source->media + i, contentBase, contentLocation, uri);
	}

	{
		AutoThreadLocker locker(s_locker);
		// TODO:
		// 1. checkout source count
		// 2. delete unused source
		s_sources[uri] = source;
	}

	return rtsp_server_reply_announce(rtsp, 200);
}

static int rtsp_find_media(const char* uri, std::shared_ptr<rtsp_source_t>& source)
{
	std::map<std::string, std::shared_ptr<rtsp_source_t> >::const_iterator it;
	for (it = s_sources.begin(); it != s_sources.end(); ++it)
	{
		source = it->second;
		for (int i = 0; i < source->count; i++)
		{
			if (0 == strcmp(source->media[i].uri, uri))
				return i;
		}
	}

	return -1;
}

static int rtsp_onsetup(void* /*ptr*/, rtsp_server_t* rtsp, const char* uri, const char* session, const struct rtsp_header_transport_t transports[], size_t num)
{
	std::string filename;
	char rtsp_transport[128] = { 0 };

	rtsp_uri_parse(uri, filename);

	std::shared_ptr<rtsp_source_t> source;
	int i = rtsp_find_media(uri, source);
	if(-1 == i)
		return rtsp_server_reply_setup(rtsp, 404 /*Not Found*/, NULL, NULL);

	std::shared_ptr<rtsp_stream_t> stream(new rtsp_stream_t);
	stream->media.reset(new rtsp_media_t);
	memcpy(stream->media.get(), source->media + i, sizeof(struct rtsp_media_t));

	TSessions::iterator it;
	AutoThreadLocker locker(s_locker);
	if (session)
	{
		it = s_sessions.find(session);
		if (it == s_sessions.end())
		{
			// 454 Session Not Found
			return rtsp_server_reply_setup(rtsp, 454, NULL, NULL);
		}
		else
		{
			// don't support aggregate control
			if (0)
			{
				// 459 Aggregate Operation Not Allowed
				return rtsp_server_reply_setup(rtsp, 459, NULL, NULL);
			}
		}
	}
	else
	{
		char rtspsession[32];
		std::shared_ptr<rtsp_session_t> item(new rtsp_session_t);
		snprintf(rtspsession, sizeof(rtspsession), "%p", item.get());
		it = s_sessions.insert(std::make_pair(rtspsession, item)).first;
	}

	for (size_t i = 0; i < num; i++)
	{
		const struct rtsp_header_transport_t* t = transports + i;
		if (RTSP_TRANSPORT_RTP_TCP == transports[i].transport)
		{
			// RTP/AVP/TCP
			// 10.12 Embedded (Interleaved) Binary Data (p40)
			memcpy(&stream->transport, t, sizeof(rtsp_header_transport_t));
			// RTP/AVP/TCP;interleaved=0-1
			snprintf(rtsp_transport, sizeof(rtsp_transport), "RTP/AVP/TCP;interleaved=%d-%d", stream->transport.interleaved1, stream->transport.interleaved2);
			break;
		}
		else if (RTSP_TRANSPORT_RTP_UDP == transports[i].transport)
		{
			// RTP/AVP/UDP
			memcpy(&stream->transport, t, sizeof(rtsp_header_transport_t));

			if (t->multicast)
			{
				// RFC 2326 1.6 Overall Operation p12
				// Multicast, client chooses address
				// Multicast, server chooses address
				assert(0);
				// 461 Unsupported Transport
				return rtsp_server_reply_setup(rtsp, 461, NULL, NULL);
			}
			else
			{
				// unicast
				unsigned short port[2];
				if (0 != sockpair_create(NULL, stream->socket, port))
				{
					// 500 Internal Server Error
					return rtsp_server_reply_setup(rtsp, 500, NULL, NULL);
				}

				assert(stream->transport.rtp.u.client_port1 && stream->transport.rtp.u.client_port2);
				if (0 == stream->transport.destination[0])
					snprintf(stream->transport.destination, sizeof(stream->transport.destination), "%s", rtsp_server_get_client(rtsp, NULL));

				// RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257;destination=xxxx
				snprintf(rtsp_transport, sizeof(rtsp_transport),
					"RTP/AVP;unicast;client_port=%hu-%hu;server_port=%hu-%hu%s%s",
					t->rtp.u.client_port1, t->rtp.u.client_port2,
					port[0], port[1],
					t->destination[0] ? ";destination=" : "",
					t->destination[0] ? t->destination : "");
			}
			break;
		}
		else
		{
			// 461 Unsupported Transport
			// try next
		}
	}
	if (0 == rtsp_transport[0])
	{
		// 461 Unsupported Transport
		return rtsp_server_reply_setup(rtsp, 461, NULL, NULL);
	}

	it->second->status = 0;
	it->second->streams.push_back(stream);
	return rtsp_server_reply_setup(rtsp, 200, it->first.c_str(), rtsp_transport);
}

static int rtsp_onrecord(void* /*ptr*/, rtsp_server_t* rtsp, const char* uri, const char* session, const int64_t* /*npt*/, const double* /*scale*/)
{
	std::list<std::shared_ptr<rtsp_stream_t> > streams;
	{
		TSessions::iterator it;
		AutoThreadLocker locker(s_locker);
		it = s_sessions.find(session ? session : "");
		if (it == s_sessions.end())
		{
			// 454 Session Not Found
			return rtsp_server_reply_play(rtsp, 454, NULL, NULL, NULL);
		}
		else
		{
			// uri with track
			if (0)
			{
				// 460 Only aggregate operation allowed
				return rtsp_server_reply_play(rtsp, 460, NULL, NULL, NULL);
			}
		}

		it->second->status = 1;
		streams = it->second->streams;
	}

	std::list<std::shared_ptr<rtsp_stream_t> >::iterator it;
	for (it = streams.begin(); it != streams.end(); ++it)
	{
		std::shared_ptr<rtsp_stream_t>& stream = *it;
		if (RTSP_TRANSPORT_RTP_UDP == stream->transport.transport)
		{
			assert(!stream->transport.multicast);
			int port[2] = { stream->transport.rtp.u.client_port1, stream->transport.rtp.u.client_port2 };
			rtp_receiver_test(stream->socket, stream->transport.destination, port, stream->media->avformats[0].fmt, stream->media->avformats[0].encoding);
		}
		else if (RTSP_TRANSPORT_RTP_TCP == stream->transport.transport)
		{
			assert(0);
			// to be continue
		}
		else
		{
			assert(0);
		}
	}
	
	return rtsp_server_reply_record(rtsp, 200, NULL, NULL);
}

static int rtsp_onteardown(void* /*ptr*/, rtsp_server_t* rtsp, const char* /*uri*/, const char* session)
{
	std::list<std::shared_ptr<rtsp_stream_t> > streams;
	{
		TSessions::iterator it;
		AutoThreadLocker locker(s_locker);
		it = s_sessions.find(session ? session : "");
		if (it == s_sessions.end())
		{
			// 454 Session Not Found
			return rtsp_server_reply_play(rtsp, 454, NULL, NULL, NULL);
		}
		else
		{
			// uri with track
			if (0)
			{
				// 460 Only aggregate operation allowed
				return rtsp_server_reply_play(rtsp, 460, NULL, NULL, NULL);
			}
		}

		streams = it->second->streams;
		s_sessions.erase(it);
	}

	std::list<std::shared_ptr<rtsp_stream_t> >::iterator it;
	for (it = streams.begin(); it != streams.end(); ++it)
	{
		std::shared_ptr<rtsp_stream_t>& stream = *it;
	}

	return rtsp_server_reply_teardown(rtsp, 200);
}

static int rtsp_onclose(void* /*ptr2*/)
{
	// TODO: notify rtsp connection lost
	//       start a timer to check rtp/rtcp activity
	//       close rtsp media session on expired
	printf("rtsp close\n");
	return 0;
}

static void rtsp_onerror(void* /*param*/, rtsp_server_t* rtsp, int code)
{
	printf("rtsp_onerror code=%d, rtsp=%p\n", code, rtsp);
}

#define N_AIO_THREAD 4
extern "C" void rtsp_push_server()
{
	aio_worker_init(N_AIO_THREAD);

	struct aio_rtsp_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.base.onannounce = rtsp_onannounce;
	handler.base.onsetup = rtsp_onsetup;
	handler.base.onrecord = rtsp_onrecord;
	handler.base.onteardown = rtsp_onteardown;
	handler.base.close = rtsp_onclose;
//	handler.base.send; // ignore
	handler.onerror = rtsp_onerror;

	void* tcp = rtsp_server_listen(NULL, 5540, &handler, NULL); assert(tcp);
//	void* udp = rtsp_transport_udp_create(NULL, 554, &handler, NULL); assert(udp);

	// test only
	while (1)
	{
		system_sleep(5);

		// TODO: check rtsp session activity
	}

	aio_worker_clean(N_AIO_THREAD);
	rtsp_server_unlisten(tcp);
//	rtsp_transport_udp_destroy(udp);
}
