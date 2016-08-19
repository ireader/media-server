#if defined(_DEBUG) || defined(DEBUG)
#include "cstringext.h"
#include "sys/sock.h"
#include "sys/system.h"
#include "sys/path.h"
#include "sys/sync.hpp"
#include "ctypedef.h"
#include "aio-socket.h"
#include "thread-pool.h"
#include "rtsp-server.h"
#include "ntp-time.h"
#include "rtp-profile.h"
#include "rtp-socket.h"
#include "ps-file-source.h"
#include "h264-file-source.h"
#include "url.h"
#include "path.h"
#include <map>
#include <memory>

#define PS_VOD

static int s_running;
static thread_pool_t s_pool;
static const char* s_workdir = "e:";

static ThreadLocker s_locker;

struct rtsp_session_t
{
	std::shared_ptr<IMediaSource> media;
	socket_t socket[2];
	unsigned short port[2];
	int status; // setup-init, 1-play, 2-pause
};
typedef std::map<std::string, rtsp_session_t> TSessions;
static TSessions s_sessions;

struct TFileDescription
{
	int64_t duration;
	std::string sdpmedia;
};
static std::map<std::string, TFileDescription> s_describes;

static void worker(void* param)
{
    do
    {
        aio_socket_process(2*60*1000);    
    } while(*(int*)param);
}

static int init()
{
    int cpu = (int)system_getcpucount();
    s_pool = thread_pool_create(cpu, 1, 64);
    aio_socket_init(cpu);
    
    s_running = 1;
    while(cpu-- > 0)
    {
        thread_pool_push(s_pool, worker, &s_running); // start worker
    }
    
    return 0;
}

static int cleanup()
{
    s_running = 0;
    thread_pool_destroy(s_pool);
    aio_socket_clean();
    return 0;
}

static int rtsp_uri_parse(const char* uri, std::string& path)
{
	void* parser = NULL;
	parser = url_parse(uri);
	if(!parser)
		return -1;

	path.assign(url_getpath(parser));
	url_free(parser);
	return 0;
}

static void rtsp_ondescribe(void* /*ptr*/, void* rtsp, const char* uri)
{
    static const char* pattern =
        "v=0\n"
        "o=- %llu %llu IN IP4 %s\n"
        "s=%s\n"
        "c=IN IP4 0.0.0.0\n"
        "t=0 0\n"
		"a=range:npt=0-%f\n"
        "a=recvonly\n";

    char sdp[512];
	std::string filename;
	std::map<std::string, TFileDescription>::const_iterator it;

	rtsp_uri_parse(uri, filename);
	assert(strstartswith(filename.c_str(), "/live/"));
	filename = path::join(s_workdir, filename.c_str()+6);

	{
		AutoThreadLocker locker(s_locker);
		it = s_describes.find(filename);
		if(it == s_describes.end())
		{
			// unlock
			TFileDescription describe;
			std::shared_ptr<IMediaSource> source;
#if defined(PS_VOD)
			source.reset(new PSFileSource(filename.c_str()));
#else
			source.reset(new H264FileSource(filename.c_str()));
#endif
			source->GetDuration(describe.duration);
			source->GetSDPMedia(describe.sdpmedia);

			// re-lock
			it = s_describes.insert(std::make_pair(filename, describe)).first;
		}
	}
    int offset = snprintf(sdp, sizeof(sdp), pattern, ntp64_now(), ntp64_now(), "0.0.0.0", uri, it->second.duration/1000.0);
	offset += strlcat(sdp + offset, it->second.sdpmedia.c_str(), sizeof(sdp) - offset);

    rtsp_server_reply_describe(rtsp, 200, sdp);
}

static void rtsp_onsetup(void* /*ptr*/, void* rtsp, const char* uri, const char* session, const struct rtsp_header_transport_t transports[], size_t num)
{
	std::string filename;
	char rtsp_transport[128];
	const struct rtsp_header_transport_t *transport = NULL;

	TSessions::iterator it;
	if(session)
	{
		AutoThreadLocker locker(s_locker);
		it = s_sessions.find(session);
		if(it == s_sessions.end())
		{
			// 454 Session Not Found
			rtsp_server_reply_setup(rtsp, 454, NULL, NULL);
		}
		else
		{
			// TODO:
			assert(0);

			// 459 Aggregate Operation Not Allowed
			rtsp_server_reply_setup(rtsp, 459, NULL, NULL);
		}
		return;
	}
	else
	{
		rtsp_uri_parse(uri, filename);
		assert(strstartswith(filename.c_str(), "/live/"));
		filename = path::join(s_workdir, filename.c_str()+6);
		if('\\' == *filename.rbegin() || '/' == *filename.rbegin())
			filename.erase(filename.end()-1);

		rtsp_session_t item;
		memset(&item, 0, sizeof(item));
#if defined(PS_VOD)
		item.media.reset(new PSFileSource(filename.c_str()));
#else
		item.media.reset(new H264FileSource(filename.c_str()));
#endif

		char rtspsession[32];
		snprintf(rtspsession, sizeof(rtspsession), "%p", item.media.get());

		AutoThreadLocker locker(s_locker);
		it = s_sessions.insert(std::make_pair(rtspsession, item)).first;
	}

	assert(NULL == transport);
	for(size_t i = 0; i < num; i++)
	{
		if(RTSP_TRANSPORT_RTP_UDP == transports[i].transport)
		{
			// RTP/AVP/UDP
			transport = &transports[i];
			break;
		}
		else if(RTSP_TRANSPORT_RTP_TCP == transports[i].transport)
		{
			// RTP/AVP/TCP
			// 10.12 Embedded (Interleaved) Binary Data (p40)
			transport = &transports[i];
			break;
		}
	}
	if(!transport)
	{
		// 461 Unsupported Transport
		rtsp_server_reply_setup(rtsp, 461, NULL, NULL);
		return;
	}

	if(transport->multicast)
	{
		// RFC 2326 1.6 Overall Operation p12
		// Multicast, client chooses address
		// Multicast, server chooses address
		assert(0);
	}
	else
	{
		// unicast
		assert(transport->rtp.u.client_port1 && transport->rtp.u.client_port2);

		rtsp_session_t &item = it->second;
		if(0 != rtp_socket_create(NULL, item.socket, item.port))
		{
			// log

			// 500 Internal Server Error
			rtsp_server_reply_setup(rtsp, 500, NULL, NULL);
			return;
		}

		// RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257
		snprintf(rtsp_transport, sizeof(rtsp_transport), 
			"RTP/AVP;unicast;client_port=%hu-%hu;server_port=%hu-%hu", 
			transport->rtp.u.client_port1, transport->rtp.u.client_port2,
			item.port[0], item.port[1]);

		char ipaddr[SOCKET_ADDRLEN] = { 0 };
		const char *ip = ipaddr;
		if(transport->destination[0])
		{
			ip = transport->destination;
			strcat(rtsp_transport, ";destination=");
			strcat(rtsp_transport, transport->destination);
		}
		else
		{
			unsigned short port = 0;
			rtsp_server_get_client(rtsp, ipaddr, &port);
		}

		unsigned short port[2] = { transport->rtp.u.client_port1, transport->rtp.u.client_port2 };
		item.media->SetRTPSocket(ip, item.socket, port);
	}

    rtsp_server_reply_setup(rtsp, 200, it->first.c_str(), rtsp_transport);
}

static void rtsp_onplay(void* /*ptr*/, void* rtsp, const char* uri, const char* session, const int64_t *npt, const double *scale)
{
	std::shared_ptr<IMediaSource> source;
	TSessions::iterator it;
	{
		AutoThreadLocker locker(s_locker);
		it = s_sessions.find(session ? session : "");
		if(it == s_sessions.end())
		{
			// 454 Session Not Found
			rtsp_server_reply_setup(rtsp, 454, NULL, NULL);
			return;
		}

		source = it->second.media;
	}
	if(npt && 0 != source->Seek(*npt))
	{
		// 457 Invalid Range
		rtsp_server_reply_setup(rtsp, 457, NULL, NULL);
		return;
	}

	if(scale && 0 != source->SetSpeed(*scale))
	{
		// set speed
		assert(scale > 0);

		// 406 Not Acceptable
		rtsp_server_reply_setup(rtsp, 406, NULL, NULL);
		return;
	}

	// RFC 2326 12.33 RTP-Info (p55)
	// 1. Indicates the RTP timestamp corresponding to the time value in the Range response header.
	// 2. A mapping from RTP timestamps to NTP timestamps (wall clock) is available via RTCP.
	int64_t tnow = 0;
	unsigned short seq = 0;
	unsigned int rtptime = 0;

	source->GetRTPInfo(tnow, seq, rtptime);

	char rtpinfo[128] = {0};
	// url=rtsp://video.example.com/twister/video;seq=12312232;rtptime=78712811
	snprintf(rtpinfo, sizeof(rtpinfo), "url=%s;seq=%hu;rtptime=%u", uri, seq, rtptime);

	it->second.status = 1;
	rtsp_server_reply_play(rtsp, 200, NULL, NULL, rtpinfo);
    //rtsp_server_reply_play(rtsp, 200, &tnow, NULL, rtpinfo);
}

static void rtsp_onpause(void* /*ptr*/, void* rtsp, const char* /*uri*/, const char* session, const int64_t* /*npt*/)
{
	std::shared_ptr<IMediaSource> source;
	TSessions::iterator it;
	{
		AutoThreadLocker locker(s_locker);
		it = s_sessions.find(session ? session : "");
		if(it == s_sessions.end())
		{
			// 454 Session Not Found
			rtsp_server_reply_setup(rtsp, 454, NULL, NULL);
			return;
		}

		source = it->second.media;
		it->second.status = 2;
	}

	source->Pause();

	// 457 Invalid Range

    rtsp_server_reply_pause(rtsp, 200);
}

static void rtsp_onteardown(void* /*ptr*/, void* rtsp, const char* /*uri*/, const char* session)
{
	std::shared_ptr<IMediaSource> source;
	TSessions::const_iterator it;
	{
		AutoThreadLocker locker(s_locker);
		it = s_sessions.find(session ? "" : session);
		if(it == s_sessions.end())
		{
			// 454 Session Not Found
			rtsp_server_reply_setup(rtsp, 454, NULL, NULL);
			return;
		}

		source = it->second.media;
		s_sessions.erase(it);
	}

	rtsp_server_reply_teardown(rtsp, 200);
}

extern "C" void rtsp_example()
{
    void *rtsp;
    struct rtsp_handler_t handler;

    rtsp_server_init();
    init();

    handler.describe = rtsp_ondescribe;
    handler.setup = rtsp_onsetup;
    handler.play = rtsp_onplay;
    handler.pause = rtsp_onpause;
    handler.teardown = rtsp_onteardown;
    rtsp = rtsp_server_create(NULL, 554, &handler, NULL);

    while(1)
    {
		int n = 0;
		TSessions::iterator it;
		for(it = s_sessions.begin(); it != s_sessions.end(); ++it)
		{
			rtsp_session_t &session = it->second;
			if(1 == session.status)
				n += session.media->Play();
		}

		if(0 == n)
			system_sleep(5);
    }

    rtsp_server_cleanup();
}
#endif
