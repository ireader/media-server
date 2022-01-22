#include "cpm/shared_ptr.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include "sockutil.h"
#include "sys/path.h"
#include "sys/pollfd.h"
#include "rtsp-media.h"
#include "rtcp-header.h"
#include "rtsp-demuxer.h"
#include "mov-format.h"
#include "mov-writer.h"
#include "time64.h"
#include "base64.h"
#include "avcodecid.h"
#include "avtimeline.h"
#include "rtp-profile.h"
#include "rtsp-payloads.h"

#if defined(OS_LINUX)
#include <malloc.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

extern "C" int rtsp_addr_is_multicast(const char* ip);
extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static int s_quit = 0;
static const int N = 3;

struct rtp_receiver_track_t
{
	struct rtp_receiver_t* ctx;
	
	socket_t udp[2];
	struct sockaddr_storage peer[2];
	int tracks[8];

	struct rtsp_media_t* m;
	struct rtsp_demuxer_t* demuxer;
	struct avtimeline_t line;
};

struct rtp_receiver_t
{
	int timeout;
	int duration;

	int count;
	struct rtsp_media_t media[N];
	struct rtp_receiver_track_t tracks[N];

	mov_writer_t* mov;	
};

static void PrintVersion(const char* program)
{
	printf("%s v0.2\n", path_basename(program));
}

static void Print(const char* program)
{
	printf("%s -i <base64 encoded sdp> [-timeout <seconds>] [-duration <seconds>] <mp4 filename>\n", path_basename(program));
	printf("\ne.g. %s -i <...> -timeout 10 -duration 600 1.mp4\n", path_basename(program));
}

static size_t SdpFromArgs(char sdp[], int bytes, const char* base64)
{
	if (bytes < strlen(base64))
		return -1;
	return base64_decode(sdp, base64, strlen(base64));
}

static int rtsp_receiver_onpacket(void* param, struct avpacket_t* pkt)
{
	struct rtp_receiver_track_t* t = (struct rtp_receiver_track_t*)param;

	assert(pkt->stream->stream < sizeof(t->tracks) / sizeof(t->tracks[0]));
	if (0 == t->tracks[pkt->stream->stream])
	{
		int r = avcodecid_find_by_codecid(pkt->stream->codecid);
		if (-1 == r)
		{
			printf("unknown codec: 0x%x\n", (unsigned int)pkt->stream->codecid);
			return -1;
		}
		switch (avstream_type(pkt->stream))
		{
		case AVSTREAM_VIDEO:
			r = mov_writer_add_video(t->ctx->mov, s_payloads[r].mov, pkt->stream->width, pkt->stream->height, pkt->stream->extra, pkt->stream->bytes);
			break;

		case AVSTREAM_AUDIO:
			r = mov_writer_add_audio(t->ctx->mov, s_payloads[r].mov, pkt->stream->channels, pkt->stream->sample_bits, pkt->stream->sample_rate, pkt->stream->extra, pkt->stream->bytes);
			break;

		case AVSTREAM_SUBTITLE:
			r = mov_writer_add_subtitle(t->ctx->mov, s_payloads[r].mov, pkt->stream->extra, pkt->stream->bytes);
			break;

		default:
			r = -1;
		}

		if (r < 0)
		{
			printf("unknown codec: 0x%x\n", (unsigned int)pkt->stream->codecid);
			return -1;
		}

		t->tracks[pkt->stream->stream] = r + 1;
	}

	int track = t->tracks[pkt->stream->stream] - 1;

	// map timestamp
	int discontinuity = 0;
	int64_t timestamp = avtimeline_input64(&t->line, track, pkt->dts, &discontinuity);
	if (discontinuity)
		printf("dts/pts discontinuity\n");
	return mov_writer_write(t->ctx->mov, track, pkt->data, pkt->size, timestamp + (pkt->pts - pkt->dts), timestamp, (pkt->flags & AVPACKET_FLAG_KEY) ? MOV_AV_FLAG_KEYFREAME : 0);
}

static int rtsp_receiver_sdp(struct rtp_receiver_t *ctx, const char* sdp, int len)
{
	int i, j;
	int count;
	struct rtsp_media_t* m;
	struct rtp_receiver_track_t* t;

	if (NULL == sdp || 0 == *sdp)
		return -1;

	count = rtsp_media_sdp(sdp, len, ctx->media, N);
	if (count < 0 || count > N)
	{
		printf("Invalid sdp medias\n");
		return count < 0 ? count : -E2BIG; // too many media stream
	}

	ctx->count = count;
	for (i = 0; i < ctx->count; i++)
	{
		m = ctx->media + i;
		t = ctx->tracks + i;
		
		t->m = m;
		t->ctx = ctx;
		t->udp[0] = socket_udp_bind(0, NULL, (unsigned short)m->port[0], 0, 0);
		t->udp[1] = socket_udp_bind(0, NULL, (unsigned short)m->port[1] ? (unsigned short)m->port[1] : (unsigned short)m->port[0] + 1, 0, 0);
		
		if (rtsp_addr_is_multicast(m->address))
		{
			// TODO:
		}

		socket_setrecvbuf(t->udp[0], 2 * 1024 * 1024);

		avtimeline_init(&t->line, 5000, 0);
		t->demuxer = rtsp_demuxer_create(0, 100, rtsp_receiver_onpacket, t);
		for (j = 0; j < m->avformat_count; j++)
		{
			int frequence = m->avformats[j].rate;
			if (0 == frequence)
			{
				const struct rtp_profile_t* profile;
				profile = rtp_profile_find(m->avformats[j].fmt);
				frequence = profile ? profile->frequency : 0;
			}

			if (0 != rtsp_demuxer_add_payload(t->demuxer, frequence, m->avformats[j].fmt, m->avformats[j].encoding, m->avformats[j].fmtp))
			{
				printf("add payload(%d, %d, %s, %s) failed.\n", m->avformats[j].rate, m->avformats[j].fmt, m->avformats[j].encoding, m->avformats[j].fmtp);
			}
		}
	}

	return 0;
}

static int rtsp_receiver_rtp(struct rtp_receiver_track_t* t)
{
	int r;
	char buffer[1500];
	
	struct sockaddr_storage ss;
	socklen_t len = sizeof(ss);
	r = recvfrom(t->udp[0], buffer, sizeof(buffer), 0, (struct sockaddr*)&ss, &len);
	if (r < 12)
		return -1;

	// skip peer valid check, update peer addr
	memcpy(&t->peer[0], &ss, sizeof(t->peer[0]));
	assert(0 == socket_addr_compare((const struct sockaddr*)&ss, (const struct sockaddr*)&t->peer[0]));

	return rtsp_demuxer_input(t->demuxer, buffer, r);
}

static int rtsp_receiver_rtcp(struct rtp_receiver_track_t *t)
{
	int r;
	char buffer[1500];

	struct sockaddr_storage ss;
	socklen_t len = sizeof(ss);
	r = recvfrom(t->udp[0], buffer, sizeof(buffer), 0, (struct sockaddr*)&ss, &len);
	if (r < 12)
		return -1;

	// skip peer valid check, update peer addr
	memcpy(&t->peer[1], &ss, sizeof(t->peer[0]));
	assert(0 == socket_addr_compare((const struct sockaddr*)&ss, (const struct sockaddr*)&t->peer[1]));

	r = rtsp_demuxer_input(t->demuxer, buffer, r);
	if (RTCP_BYE == r)
	{
		printf("[RTCP] receive rtcp bye, finished\n");
		s_quit = 1;
	}
	return 0;
}

static int rtsp_receiver_run(struct rtp_receiver_t* ctx)
{
	int i, r;
	char rtcp[1500];
	struct pollfd fds[2*N];
	struct rtp_receiver_track_t* t;
	
	for (i = 0; i < 2 * ctx->count; i++)
	{
		fds[i].fd = ctx->tracks[i/2].udp[i%2];
		fds[i].events = POLLIN;
		fds[i].revents = 0;
	}

	time64_t clock = 0;
	while (!s_quit)
	{
		// RTCP report
		for (i = 0; i < ctx->count; i++)
		{
			t = ctx->tracks + i;
			r = rtsp_demuxer_rtcp(t->demuxer, rtcp, sizeof(rtcp));
			if (r > 0)
				r = socket_sendto(t->udp[1], rtcp, r, 0, (const struct sockaddr*)&t->peer[1], socket_addr_len((const struct sockaddr*)&t->peer[1]));
		}

		r = poll(fds, 2 * ctx->count, ctx->timeout);
		while (-1 == r && EINTR == errno)
			r = poll(fds, 2 * ctx->count, ctx->timeout);

		if (0 == r)
		{
			printf("rtp receive timeout\n");
			return -ETIMEDOUT; // timeout
		}
		else if (r < 0)
		{
			printf("rtp receive error: %d/%d\n", r, socket_geterror());
			return r; // error
		}
		else
		{
			for (i = 0; i < ctx->count; i++)
			{
				t = ctx->tracks + i;

				if (0 != fds[2 * i + 0].revents)
				{
					rtsp_receiver_rtp(t);
					fds[2 * i + 0].revents = 0;
				}

				if (0 != fds[2 * i + 1].revents)
				{
					rtsp_receiver_rtcp(t);
					fds[2 * i + 1].revents = 0;
				}
			}
		}

		time64_t now = time64_now();
		if (0 == clock)
		{
			clock = now;
		}
		else if (now - clock > ctx->duration)
		{
			break; // exit
		}
	}

	return 0;
}

static void OnQuit(int sig)
{
	s_quit = 1;
}

int rtp_mov_test(int argc, char* argv[])
{
#if defined(OS_LINUX)
	struct rlimit limit;
	limit.rlim_cur = RLIM_INFINITY;
	limit.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &limit);

	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, 0);
	sigaction(SIGPIPE, &sa, 0);

	sa.sa_handler = OnQuit;
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGTERM, &sa, 0);
	sigaction(SIGQUIT, &sa, 0);
#elif defined(OS_WINDOWS)
	signal(SIGINT, OnQuit);
#endif
	socket_init();

	static char sdp[4 * 1024];
	const char* output = NULL;
	int faststart = 0;

	static struct rtp_receiver_t ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.timeout = 20 * 1000; // default 20s
	ctx.duration = 10 * 60 * 1000; // default 10m

	PrintVersion(argv[0]);
	for (int i = 1; i < argc; i++)
	{
		if (0 == strcmp("-i", argv[i]))
		{
			if (i + 1 < argc)
				SdpFromArgs(sdp, sizeof(sdp), argv[++i]);
		}
		else if (0 == strcmp("-timeout", argv[i]))
		{
			if (i + 1 < argc)
				ctx.timeout = 1000 * (int)strtoul(argv[++i], NULL, 10);
		}
		else if (0 == strcmp("-duration", argv[i]))
		{
			if (i + 1 < argc)
				ctx.duration = 1000 * (int)strtoul(argv[++i], NULL, 10);
		}
		else if (0 == strncmp("-faststart", argv[i], 1))
		{
			faststart = 1;
		}
		else if(0 == strncmp("-", argv[i], 1))
		{
			++i; // ignore
		}
		else
		{
			output = argv[i];
		}
	}

	if (!output || !*output || !*sdp || ctx.timeout < 0 || ctx.duration < 0)
	{
		Print(argv[0]);
		exit(-1);
	}

	std::shared_ptr<FILE> fp(fopen(output, "wb"), fclose);
	if (!fp)
	{
		printf("create mp4 file: %s failed: %d\n", output, errno);
		exit(-1);
	}

	printf("sdp: \n%s\n", sdp);
	ctx.mov = mov_writer_create(mov_file_buffer(), fp.get(), faststart ? MOV_FLAG_FASTSTART : 0);
	rtsp_receiver_sdp(&ctx, sdp, strlen(sdp));
	rtsp_receiver_run(&ctx);

	mov_writer_destroy(ctx.mov);
	socket_cleanup();
	return 0;
}
