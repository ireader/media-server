#if defined(_DEBUG) || defined(DEBUG)

#include "sockutil.h"
#include "rtsp-client.h"
#include <assert.h>
#include <stdlib.h>
#include "sockpair.h"
#include "cstringext.h"
#include "sys/system.h"
#include "cpm/unuse.h"
#include "sdp.h"
#include "ntp-time.h"
#include "rtp-profile.h"
#include "media/ps-file-source.h"
#include "media/h264-file-source.h"
#include "media/h265-file-source.h"
#include "media/mp4-file-source.h"
#include "rtp-udp-transport.h"
#include <map>
#include <memory>
#include <string>
#include "cpm/shared_ptr.h"

#if defined(_HAVE_FFMPEG_)
#include "media/ffmpeg-file-source.h"
#include "media/ffmpeg-live-source.h"
#endif

//#define UDP_MULTICAST_ADDR "239.0.0.2"

extern "C" int rtsp_addr_is_multicast(const char* ip);

struct rtsp_client_push_test_t
{
	std::shared_ptr<IMediaSource> source;
	rtsp_client_t* rtsp;
	socket_t socket;
	char sdp[2 * 1024];
	std::string peer;

	int mode;
	int status;
	socket_t rtp[5][2];
	unsigned short port[5][2];
	std::shared_ptr<IRTPTransport> transport[5];
};

static int rtsp_client_sdp(struct rtsp_client_push_test_t* ctx, const char* file)
{
	static const char* pattern_vod =
		"v=0\n"
		"o=- %llu %llu IN IP4 %s\n"
		"s=rtsp-client-push-test\n"
		"c=IN IP4 0.0.0.0\n"
		"t=0 0\n"
		"a=range:npt=0-%.1f\n"
		"a=recvonly\n"
		"a=control:*\n"; // aggregate control

	static const char* pattern_live =
		"v=0\n"
		"o=- %llu %llu IN IP4 %s\n"
		"s=rtsp-client-push-test\n"
		"c=IN IP4 0.0.0.0\n"
		"t=0 0\n"
		"a=range:npt=now-\n" // live
		"a=recvonly\n"
		"a=control:*\n"; // aggregate control

	int offset = 0;
	if (0 == strcmp(file, "camera"))
	{
#if defined(_HAVE_FFMPEG_)
		ctx->source.reset(new FFLiveSource("video=Integrated Webcam"));
#endif
		offset = snprintf(ctx->sdp, sizeof(ctx->sdp), pattern_live, ntp64_now(), ntp64_now(), "0.0.0.0");
		assert(offset > 0 && offset + 1 < sizeof(ctx->sdp));
	}
	else
	{
		if (strendswith(file, ".ps"))
			ctx->source.reset(new PSFileSource(file));
		else if (strendswith(file, ".h264"))
			ctx->source.reset(new H264FileSource(file));
		else if (strendswith(file, ".h265"))
			ctx->source.reset(new H265FileSource(file));
		else
		{
#if defined(_HAVE_FFMPEG_)
			ctx->source.reset(new FFFileSource(file));
#else
			ctx->source.reset(new MP4FileSource(file));
#endif
		}

		int64_t duration;
		ctx->source->GetDuration(duration);

		offset = snprintf(ctx->sdp, sizeof(ctx->sdp), pattern_vod, ntp64_now(), ntp64_now(), "0.0.0.0", duration / 1000.0);
		assert(offset > 0 && offset + 1 < sizeof(ctx->sdp));
	}

	std::string sdpmedia;
	ctx->source->GetSDPMedia(sdpmedia);
	return offset + snprintf(ctx->sdp + offset, sizeof(ctx->sdp) - offset, "%s", sdpmedia.c_str());
}

static int rtsp_client_send(void* param, const char* uri, const void* req, size_t bytes)
{
	//TODO: check uri and make socket
	//1. uri != rtsp describe uri(user input)
	//2. multi-uri if media_count > 1
	struct rtsp_client_push_test_t* ctx = (struct rtsp_client_push_test_t*)param;
	return socket_send_all_by_time(ctx->socket, req, bytes, 0, 2000);
}

static int rtpport(void* param, int media, const char* source, unsigned short rtp[2], char* ip, int len)
{
	struct rtsp_client_push_test_t* ctx = (struct rtsp_client_push_test_t*)param;
	int m = rtsp_client_get_media_type(ctx->rtsp, media);
	if (SDP_M_MEDIA_AUDIO != m && SDP_M_MEDIA_VIDEO != m)
		return 0; // ignore

	switch (ctx->mode)
	{
	case RTSP_TRANSPORT_RTP_UDP:
		// TODO: ipv6
		assert(0 == sockpair_create("0.0.0.0", ctx->rtp[media], ctx->port[media]));
		rtp[0] = ctx->port[media][0];
		rtp[1] = ctx->port[media][1];

		if (rtsp_addr_is_multicast(ip))
		{
			if (0 != socket_udp_multicast(ctx->rtp[media][0], ip, source, 16) || 0 != socket_udp_multicast(ctx->rtp[media][1], ip, source, 16))
				return -1;
		}
#if defined(UDP_MULTICAST_ADDR)
		else
		{
			if (0 != socket_udp_multicast(ctx->rtp[media][0], UDP_MULTICAST_ADDR, source, 16) || 0 != socket_udp_multicast(ctx->rtp[media][1], UDP_MULTICAST_ADDR, source, 16))
				return -1;
			snprintf(ip, len, "%s", UDP_MULTICAST_ADDR);
		}
#endif
		break;

	case RTSP_TRANSPORT_RTP_TCP:
		rtp[0] = 2 * media;
		rtp[1] = 2 * media + 1;
		break;

	default:
		assert(0);
		return -1;
	}

	return ctx->mode;
}

int rtsp_client_options(rtsp_client_t* rtsp, const char* commands);
static void onrtp(void* param, uint8_t channel, const void* data, uint16_t bytes)
{
	static int keepalive = 0;
	struct rtsp_client_push_test_t* ctx = (struct rtsp_client_push_test_t*)param;
	//rtp_receiver_tcp_input(channel, data, bytes);
	//if (++keepalive % 1000 == 0)
	//{
	//	rtsp_client_play(ctx->rtsp, NULL, NULL);
	//}
}

static int onannounce(void* param)
{
	struct rtsp_client_push_test_t* ctx = (struct rtsp_client_push_test_t*)param;
	return rtsp_client_setup(ctx->rtsp, ctx->sdp, strlen(ctx->sdp));
}

static int onsetup(void* param, int timeout, int64_t duration)
{
	int i;
	uint64_t npt = 0;
	struct rtsp_client_push_test_t* ctx = (struct rtsp_client_push_test_t*)param;
	for (i = 0; i < rtsp_client_media_count(ctx->rtsp); i++)
	{
		int payload;
		unsigned short port[2];
		const char* encoding;
		const struct rtsp_header_transport_t* transport;

		char track[16] = { 0 };
#if defined(_HAVE_FFMPEG_)
		snprintf(track, sizeof(track) - 1, "track%d", i);
#else
		snprintf(track, sizeof(track) - 1, "track%d", i + 1); // mp4 track base 1
#endif

		transport = rtsp_client_get_media_transport(ctx->rtsp, i);
		encoding = rtsp_client_get_media_encoding(ctx->rtsp, i);
		payload = rtsp_client_get_media_payload(ctx->rtsp, i);
		if (RTSP_TRANSPORT_RTP_UDP == transport->transport)
		{
			//assert(RTSP_TRANSPORT_RTP_UDP == transport->transport); // udp only
			assert(0 == transport->multicast); // unicast only
			assert(transport->rtp.u.client_port1 == ctx->port[i][0]);
			assert(transport->rtp.u.client_port2 == ctx->port[i][1]);

			port[0] = transport->rtp.u.server_port1;
			port[1] = transport->rtp.u.server_port2;
			ctx->transport[i] = std::make_shared<RTPUdpTransport>();
			assert(transport->rtp.u.server_port1 && transport->rtp.u.server_port2);
			const char* ip = transport->destination[0] ? transport->destination : ctx->peer.c_str();
			assert(0 == ((RTPUdpTransport*)ctx->transport[i].get())->Init(ctx->rtp[i], ip, port));	
			ctx->source->SetTransport(track, ctx->transport[i]);
		}
		else if (RTSP_TRANSPORT_RTP_TCP == transport->transport)
		{
			//assert(transport->rtp.u.client_port1 == transport->interleaved1);
			//assert(transport->rtp.u.client_port2 == transport->interleaved2);
			assert(0); // todo
		}
		else
		{
			assert(0); // TODO
		}
	}

	assert(0 == rtsp_client_record(ctx->rtsp, &npt, NULL));
	return 0;
}

static int onteardown(void* param)
{
	return 0;
}

static int onrecord(void* param, int media, const uint64_t* nptbegin, const uint64_t* nptend, const double* scale, const struct rtsp_rtp_info_t* rtpinfo, int count)
{
	struct rtsp_client_push_test_t* ctx = (struct rtsp_client_push_test_t*)param;
	ctx->status = 1;
	return 0;
}

void rtsp_client_push_test(const char* host, const char* file)
{
	int r;
	struct rtsp_client_push_test_t ctx;
	struct rtsp_client_handler_t handler;
	static char packet[2 * 1024 * 1024];

	memset(&ctx, 0, sizeof(ctx));
	handler.send = rtsp_client_send;
	handler.rtpport = rtpport;
	handler.onannounce = onannounce;
	handler.onsetup = onsetup;
	handler.onrecord = onrecord;
	handler.onteardown = onteardown;
	handler.onrtp = onrtp;

	ctx.status = 0;
	ctx.peer = std::string(host);
	ctx.mode = RTSP_TRANSPORT_RTP_UDP; // RTSP_TRANSPORT_RTP_TCP
	snprintf(packet, sizeof(packet), "rtsp://%s/live/push", host); // url

	socket_init();
	ctx.socket = socket_connect_host(host, 554, 2000);
	assert(socket_invalid != ctx.socket);
	//ctx.rtsp = rtsp_client_create(NULL, NULL, &handler, &ctx);
	ctx.rtsp = rtsp_client_create(packet, "username1", "password1", &handler, &ctx);
	assert(ctx.rtsp);
	assert(rtsp_client_sdp(&ctx, file) > 0);
	assert(0 == rtsp_client_announce(ctx.rtsp, ctx.sdp));

	socket_setnonblock(ctx.socket, 0);
	r = socket_recv(ctx.socket, packet, sizeof(packet), 0);
	while (r > 0)
	{
		assert(0 == rtsp_client_input(ctx.rtsp, packet, r));
		if (ctx.status == 1)
			break;
		r = socket_recv(ctx.socket, packet, sizeof(packet), 0);
	}

	while (r > 0 && ctx.status == 1)
	{
		system_sleep(5);

		if (1 == ctx.status)
			ctx.source->Play();

		// TODO: check rtsp session activity
	}

	assert(0 == rtsp_client_teardown(ctx.rtsp));
	rtsp_client_destroy(ctx.rtsp);
	socket_close(ctx.socket);
	socket_cleanup();
}

#endif
