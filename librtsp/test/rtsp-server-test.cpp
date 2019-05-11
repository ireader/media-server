#if defined(_DEBUG) || defined(DEBUG)
#include "cstringext.h"
#include "sys/sock.h"
#include "sys/system.h"
#include "sys/path.h"
#include "sys/sync.hpp"
#include "aio-worker.h"
#include "ctypedef.h"
#include "ntp-time.h"
#include "rtp-profile.h"
#include "rtsp-server.h"
#include "media/ps-file-source.h"
#include "media/h264-file-source.h"
#include "media/mp4-file-source.h"
#include "rtp-udp-transport.h"
#include "rtp-tcp-transport.h"
#include "rtsp-server-aio.h"
#include "uri-parse.h"
#include "urlcodec.h"
#include "path.h"
#include <map>
#include <memory>
#include "cpm/shared_ptr.h"

#if defined(_HAVE_FFMPEG_)
#include "media/ffmpeg-file-source.h"
#include "media/ffmpeg-live-source.h"
#endif

static const char* s_workdir = "e:\\";

static ThreadLocker s_locker;

struct rtsp_media_t
{
	std::shared_ptr<IMediaSource> media;
	std::shared_ptr<IRTPTransport> transport;
	uint8_t channel; // rtp over rtsp interleaved channel
	int status; // setup-init, 1-play, 2-pause
};
typedef std::map<std::string, rtsp_media_t> TSessions;
static TSessions s_sessions;

struct TFileDescription
{
	int64_t duration;
	std::string sdpmedia;
};
static std::map<std::string, TFileDescription> s_describes;

static int rtsp_uri_parse(const char* uri, std::string& path)
{
	char path1[256];
	struct uri_t* r = uri_parse(uri, strlen(uri));
	if(!r)
		return -1;

	url_decode(r->path, strlen(r->path), path1, sizeof(path1));
	path = path1;
	uri_free(r);
	return 0;
}

static int rtsp_ondescribe(void* /*ptr*/, rtsp_server_t* rtsp, const char* uri)
{
	static const char* pattern_vod =
		"v=0\n"
		"o=- %llu %llu IN IP4 %s\n"
		"s=%s\n"
		"c=IN IP4 0.0.0.0\n"
		"t=0 0\n"
		"a=range:npt=0-%.1f\n"
		"a=recvonly\n"
		"a=control:*\n"; // aggregate control

	static const char* pattern_live =
		"v=0\n"
		"o=- %llu %llu IN IP4 %s\n"
		"s=%s\n"
		"c=IN IP4 0.0.0.0\n"
		"t=0 0\n"
		"a=range:npt=now-\n" // live
		"a=recvonly\n"
		"a=control:*\n"; // aggregate control

    std::string filename;
	std::map<std::string, TFileDescription>::const_iterator it;

	rtsp_uri_parse(uri, filename);
	if (strstartswith(filename.c_str(), "/live/"))
	{
		filename = filename.c_str() + 6;
	}
	else if (strstartswith(filename.c_str(), "/vod/"))
	{
		filename = path::join(s_workdir, filename.c_str() + 5);
	}
	else
	{
		assert(0);
		return -1;
	}

	char buffer[1024] = { 0 };
	{
		AutoThreadLocker locker(s_locker);
		it = s_describes.find(filename);
		if(it == s_describes.end())
		{
			// unlock
			TFileDescription describe;
			std::shared_ptr<IMediaSource> source;
			if (0 == strcmp(filename.c_str(), "camera"))
			{
#if defined(_HAVE_FFMPEG_)
				source.reset(new FFLiveSource("video=Integrated Webcam"));
#endif
				int offset = snprintf(buffer, sizeof(buffer), pattern_live, ntp64_now(), ntp64_now(), "0.0.0.0", uri);
				assert(offset > 0 && offset + 1 < sizeof(buffer));
			}
			else
			{
				if (strendswith(filename.c_str(), ".ps"))
					source.reset(new PSFileSource(filename.c_str()));
				else if (strendswith(filename.c_str(), ".h264"))
					source.reset(new H264FileSource(filename.c_str()));
				else
				{
#if defined(_HAVE_FFMPEG_)
					source.reset(new FFFileSource(filename.c_str()));
#else
					source.reset(new MP4FileSource(filename.c_str()));
#endif
				}
				source->GetDuration(describe.duration);

				int offset = snprintf(buffer, sizeof(buffer), pattern_vod, ntp64_now(), ntp64_now(), "0.0.0.0", uri, describe.duration / 1000.0);
				assert(offset > 0 && offset + 1 < sizeof(buffer));
			}

			source->GetSDPMedia(describe.sdpmedia);

			// re-lock
			it = s_describes.insert(std::make_pair(filename, describe)).first;
		}
	}
    
	std::string sdp = buffer;
	sdp += it->second.sdpmedia;
    return rtsp_server_reply_describe(rtsp, 200, sdp.c_str());
}

static int rtsp_onsetup(void* /*ptr*/, rtsp_server_t* rtsp, const char* uri, const char* session, const struct rtsp_header_transport_t transports[], size_t num)
{
	std::string filename;
	char rtsp_transport[128];
	const struct rtsp_header_transport_t *transport = NULL;

	rtsp_uri_parse(uri, filename);
	if (strstartswith(filename.c_str(), "/live/"))
	{
		filename = filename.c_str() + 6;
	}
	else if (strstartswith(filename.c_str(), "/vod/"))
	{
		filename = path::join(s_workdir, filename.c_str() + 5);
	}
	else
	{
		assert(0);
		return -1;
	}

	if ('\\' == *filename.rbegin() || '/' == *filename.rbegin())
		filename.erase(filename.end() - 1);
	const char* basename = path_basename(filename.c_str());
	if (NULL == strchr(basename, '.')) // filter track1
		filename.erase(basename - filename.c_str() - 1, std::string::npos);

	TSessions::iterator it;
	if(session)
	{
		AutoThreadLocker locker(s_locker);
		it = s_sessions.find(session);
		if(it == s_sessions.end())
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
		rtsp_media_t item;
		item.channel = 0;
		item.status = 0;

		if (0 == strcmp(filename.c_str(), "camera"))
		{
#if defined(_HAVE_FFMPEG_)
			item.media.reset(new FFLiveSource("video=Integrated Webcam"));
#endif
		}
		else
		{
			if (strendswith(filename.c_str(), ".ps"))
				item.media.reset(new PSFileSource(filename.c_str()));
			else if (strendswith(filename.c_str(), ".h264"))
				item.media.reset(new H264FileSource(filename.c_str()));
			else
			{
#if defined(_HAVE_FFMPEG_)
				item.media.reset(new FFFileSource(filename.c_str()));
#else
				item.media.reset(new MP4FileSource(filename.c_str()));
#endif
			}
		}

		char rtspsession[32];
		snprintf(rtspsession, sizeof(rtspsession), "%p", item.media.get());

		AutoThreadLocker locker(s_locker);
		it = s_sessions.insert(std::make_pair(rtspsession, item)).first;
	}

	assert(NULL == transport);
	for(size_t i = 0; i < num && !transport; i++)
	{
		if(RTSP_TRANSPORT_RTP_UDP == transports[i].transport)
		{
			// RTP/AVP/UDP
			transport = &transports[i];
		}
		else if(RTSP_TRANSPORT_RTP_TCP == transports[i].transport)
		{
			// RTP/AVP/TCP
			// 10.12 Embedded (Interleaved) Binary Data (p40)
			transport = &transports[i];
		}
	}
	if(!transport)
	{
		// 461 Unsupported Transport
		return rtsp_server_reply_setup(rtsp, 461, NULL, NULL);
	}

	rtsp_media_t &item = it->second;
	if (RTSP_TRANSPORT_RTP_TCP == transport->transport)
	{
		// 10.12 Embedded (Interleaved) Binary Data (p40)
		int interleaved[2];
		if (transport->interleaved1 == transport->interleaved2)
		{
			interleaved[0] = item.channel++;
			interleaved[1] = item.channel++;
		}
		else
		{
			interleaved[0] = transport->interleaved1;
			interleaved[1] = transport->interleaved2;
		}

		item.transport = std::make_shared<RTPTcpTransport>(rtsp, interleaved[0], interleaved[1]);
		item.media->SetTransport(path_basename(uri), item.transport);

		// RTP/AVP/TCP;interleaved=0-1
		snprintf(rtsp_transport, sizeof(rtsp_transport), "RTP/AVP/TCP;interleaved=%d-%d", interleaved[0], interleaved[1]);		
	}
	else if(transport->multicast)
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
		item.transport = std::make_shared<RTPUdpTransport>();

		assert(transport->rtp.u.client_port1 && transport->rtp.u.client_port2);
		unsigned short port[2] = { transport->rtp.u.client_port1, transport->rtp.u.client_port2 };
		const char *ip = transport->destination[0] ? transport->destination : rtsp_server_get_client(rtsp, NULL);
		if(0 != ((RTPUdpTransport*)item.transport.get())->Init(ip, port))
		{
			// log

			// 500 Internal Server Error
			return rtsp_server_reply_setup(rtsp, 500, NULL, NULL);
		}
		item.media->SetTransport(path_basename(uri), item.transport);

		// RTP/AVP;unicast;client_port=4588-4589;server_port=6256-6257;destination=xxxx
		snprintf(rtsp_transport, sizeof(rtsp_transport), 
			"RTP/AVP;unicast;client_port=%hu-%hu;server_port=%hu-%hu%s%s", 
			transport->rtp.u.client_port1, transport->rtp.u.client_port2,
			port[0], port[1],
			transport->destination[0] ? ";destination=" : "",
			transport->destination[0] ? transport->destination : "");
	}

    return rtsp_server_reply_setup(rtsp, 200, it->first.c_str(), rtsp_transport);
}

static int rtsp_onplay(void* /*ptr*/, rtsp_server_t* rtsp, const char* uri, const char* session, const int64_t *npt, const double *scale)
{
	std::shared_ptr<IMediaSource> source;
	TSessions::iterator it;
	{
		AutoThreadLocker locker(s_locker);
		it = s_sessions.find(session ? session : "");
		if(it == s_sessions.end())
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

		source = it->second.media;
	}
	if(npt && 0 != source->Seek(*npt))
	{
		// 457 Invalid Range
		return rtsp_server_reply_play(rtsp, 457, NULL, NULL, NULL);
	}

	if(scale && 0 != source->SetSpeed(*scale))
	{
		// set speed
		assert(*scale > 0);

		// 406 Not Acceptable
		return rtsp_server_reply_play(rtsp, 406, NULL, NULL, NULL);
	}

	// RFC 2326 12.33 RTP-Info (p55)
	// 1. Indicates the RTP timestamp corresponding to the time value in the Range response header.
	// 2. A mapping from RTP timestamps to NTP timestamps (wall clock) is available via RTCP.
	char rtpinfo[512] = { 0 };
	source->GetRTPInfo(uri, rtpinfo, sizeof(rtpinfo));

	it->second.status = 1;
    return rtsp_server_reply_play(rtsp, 200, npt, NULL, rtpinfo);
}

static int rtsp_onpause(void* /*ptr*/, rtsp_server_t* rtsp, const char* /*uri*/, const char* session, const int64_t* /*npt*/)
{
	std::shared_ptr<IMediaSource> source;
	TSessions::iterator it;
	{
		AutoThreadLocker locker(s_locker);
		it = s_sessions.find(session ? session : "");
		if(it == s_sessions.end())
		{
			// 454 Session Not Found
			return rtsp_server_reply_pause(rtsp, 454);
		}
		else
		{
			// uri with track
			if (0)
			{
				// 460 Only aggregate operation allowed
				return rtsp_server_reply_pause(rtsp, 460);
			}
		}

		source = it->second.media;
		it->second.status = 2;
	}

	source->Pause();

	// 457 Invalid Range

    return rtsp_server_reply_pause(rtsp, 200);
}

static int rtsp_onteardown(void* /*ptr*/, rtsp_server_t* rtsp, const char* /*uri*/, const char* session)
{
	std::shared_ptr<IMediaSource> source;
	TSessions::iterator it;
	{
		AutoThreadLocker locker(s_locker);
		it = s_sessions.find(session ? session : "");
		if(it == s_sessions.end())
		{
			// 454 Session Not Found
			return rtsp_server_reply_teardown(rtsp, 454);
		}

		source = it->second.media;
		s_sessions.erase(it);
	}

	return rtsp_server_reply_teardown(rtsp, 200);
}

static int rtsp_onannounce(void* /*ptr*/, rtsp_server_t* rtsp, const char* uri, const char* sdp)
{
    return rtsp_server_reply_announce(rtsp, 200);
}

static int rtsp_onrecord(void* /*ptr*/, rtsp_server_t* rtsp, const char* uri, const char* session, const int64_t *npt, const double *scale)
{
    return rtsp_server_reply_record(rtsp, 200, NULL, NULL);
}

static int rtsp_onoptions(void* ptr, rtsp_server_t* rtsp, const char* uri)
{
	const char* require = rtsp_server_get_header(rtsp, "Require");
	return rtsp_server_reply_options(rtsp, 200);
}

static int rtsp_ongetparameter(void* ptr, rtsp_server_t* rtsp, const char* uri, const char* session, const void* content, int bytes)
{
	const char* ctype = rtsp_server_get_header(rtsp, "Content-Type");
	const char* encoding = rtsp_server_get_header(rtsp, "Content-Encoding");
	const char* language = rtsp_server_get_header(rtsp, "Content-Language");
	return rtsp_server_reply_get_parameter(rtsp, 200, NULL, 0);
}

static int rtsp_onsetparameter(void* ptr, rtsp_server_t* rtsp, const char* uri, const char* session, const void* content, int bytes)
{
	const char* ctype = rtsp_server_get_header(rtsp, "Content-Type");
	const char* encoding = rtsp_server_get_header(rtsp, "Content-Encoding");
	const char* language = rtsp_server_get_header(rtsp, "Content-Language");
	return rtsp_server_reply_set_parameter(rtsp, 200);
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
extern "C" void rtsp_example()
{
	aio_worker_init(N_AIO_THREAD);

	struct aio_rtsp_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.base.ondescribe = rtsp_ondescribe;
    handler.base.onsetup = rtsp_onsetup;
    handler.base.onplay = rtsp_onplay;
    handler.base.onpause = rtsp_onpause;
    handler.base.onteardown = rtsp_onteardown;
	handler.base.close = rtsp_onclose;
    handler.base.onannounce = rtsp_onannounce;
    handler.base.onrecord = rtsp_onrecord;
	handler.base.onoptions = rtsp_onoptions;
	handler.base.ongetparameter = rtsp_ongetparameter;
	handler.base.onsetparameter = rtsp_onsetparameter;
//	handler.base.send; // ignore
	handler.onerror = rtsp_onerror;
    
	void* tcp = rtsp_server_listen(NULL, 554, &handler, NULL); assert(tcp);
//	void* udp = rtsp_transport_udp_create(NULL, 554, &handler, NULL); assert(udp);

	// test only
    while(1)
    {
		system_sleep(5);

		TSessions::iterator it;
		AutoThreadLocker locker(s_locker);
		for(it = s_sessions.begin(); it != s_sessions.end(); ++it)
		{
			rtsp_media_t &session = it->second;
			if(1 == session.status)
				session.media->Play();
		}

		// TODO: check rtsp session activity
    }

	aio_worker_clean(N_AIO_THREAD);
	rtsp_server_unlisten(tcp);
//	rtsp_transport_udp_destroy(udp);
}
#endif
