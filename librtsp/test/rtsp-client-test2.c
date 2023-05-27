#include "sockutil.h"
#include "sys/pollfd.h"
#include "uri-parse.h"
#include "rtsp-media.h"
#include "rtp-profile.h"
#include "rtcp-header.h"
#include "rtsp-client.h"
#include "rtsp-demuxer.h"
#include "avpkt2bs.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include "sockpair.h"
#include "cstringext.h"
#include "sys/system.h"
#include "cpm/unuse.h"
#include "time64.h"
#include "sdp.h"

#define PORT_RTSP 554
#define N_MEDIA		3

const struct rtsp_media_t* rtsp_client_get_media(struct rtsp_client_t* rtsp, int media);

typedef int (*rtsp_client_test2_handler)(struct rtsp_client_test2_t* ctx, socket_t socket, int media);

struct rtsp_client_test2_t
{
	void* rtsp;
	char buffer[4 * 1024];
	socket_t socket;

	int transport;
	struct avpkt2bs_t bs;

	int fds_count;
	struct pollfd fds[1 + N_MEDIA * 2];
	rtsp_client_test2_handler handlers[1 + N_MEDIA * 2];

	int fds2media[N_MEDIA * 2];
	struct rtsp_demuxer_t* demuxer[N_MEDIA];
};

static int rtsp_client_test2_onpacket(void* param, struct avpacket_t* pkt)
{
	int r;
	const uint8_t start_code[] = { 0, 0, 0, 1 };
	struct rtsp_client_test2_t* ctx;
	ctx = (struct rtsp_client_test2_t*)param;

	printf("[RTSP] packet codec: %d, pts: %" PRId64 ", dts:%" PRId64 ",bytes: % d.\n", pkt->stream->codecid, pkt->pts, pkt->dts, pkt->size);

	r = avpkt2bs_input(&ctx->bs, pkt);
	if (r < 0)
	{
		printf("[RTSP] discard packet codec: %d, pts: %" PRId64 ", dts:%" PRId64 ",bytes: % d.\n", pkt->stream->codecid, pkt->pts, pkt->dts, pkt->size);
		return 0; // discard
	}

	switch (pkt->stream->codecid)
	{
	case AVCODEC_AUDIO_AAC:
		//aac_decode(aac, ctx->bs.ptr, r);
		break;

	case AVCODEC_VIDEO_H264:
		//h264_decode(aac, ctx->bs.ptr, r);
		break;

	case AVCODEC_VIDEO_H265:
		//h265_decode(aac, ctx->bs.ptr, r);
		break;

	default:
		break;
	}

	return 0;
}

static int rtsp_client_test2_run(struct rtsp_client_test2_t* ctx)
{
	int i, r;
	time64_t clock;
	clock = time64_now();
	while (1)
	{
		r = poll(ctx->fds, ctx->fds_count, 1000);
		while (-1 == r && EINTR == errno)
			r = poll(ctx->fds, ctx->fds_count, 1000);

		if (0 == r)
		{
			continue; // timeout
		}
		else if (r < 0)
		{
			printf("[RTSP] poll read error: %d\n", r);
			return r; // error
		}
		else
		{
			for (i = 0; i < ctx->fds_count; i++)
			{
				if (0 != ctx->fds[i].revents)
				{
					r = ctx->handlers[i](ctx, ctx->fds[i].fd, ctx->fds2media[i]);
					if (0 != r)
					{
						printf("[RTSP] media[%d], socket[%d] handle error: %d\n", ctx->fds2media[i], i, r);
					}

					ctx->fds[i].revents = 0;
				}
			}
		}
	}

	return r;
}

static int rtsp_client_test2_onrtsp(struct rtsp_client_test2_t* ctx, socket_t socket, int media)
{
	int r;
	r = socket_recv(socket, ctx->buffer, sizeof(ctx->buffer), 0);
	while (r > 0)
	{
		r = rtsp_client_input(ctx->rtsp, ctx->buffer, r);
		if (0 != r)
			break;

		r = socket_recv(socket, ctx->buffer, sizeof(ctx->buffer), 0);
	}

#if defined(OS_WINDOWS)
	return r < 0 ? (socket_geterror() == WSAEWOULDBLOCK ? 0 : r) : 0;
#else
	return r < 0 ? (errno == EAGAIN || errno == EWOULDBLOCK ? 0 : r) : 0;
#endif
}

static int rtsp_client_test2_onrtp(struct rtsp_client_test2_t* ctx, socket_t socket, int media)
{
	int r;
	socklen_t len;
	struct sockaddr_storage ss;

	len = sizeof(ss);
	r = recvfrom(socket, ctx->buffer, sizeof(ctx->buffer), 0, (struct sockaddr*)&ss, &len);
	if (r < 12)
	{
		printf("[RTSP] track(%d) recv error: %d\n", media, r);
		return -1;
	}
		
	//assert(0 == socket_addr_compare((const struct sockaddr*)&ss, (const struct sockaddr*)&ctx->ss[0]));

	if (media >= sizeof(ctx->demuxer) / sizeof(ctx->demuxer[0]) || ctx->demuxer[media] == NULL)
	{
		printf("[RTSP] track(%d) discard rtp packet\n", media);
		return 0; // discard
	}
	r = rtsp_demuxer_input(ctx->demuxer[media], ctx->buffer, r);
	return r < 0 ? r : 0;
}

static int rtsp_client_test2_onrtcp(struct rtsp_client_test2_t* ctx, socket_t socket, int media)
{
	int r;
	socklen_t len;
	struct sockaddr_storage ss;

	len = sizeof(ss);
	r = recvfrom(socket, ctx->buffer, sizeof(ctx->buffer), 0, (struct sockaddr*)&ss, &len);
	if (r < 12)
	{
		printf("[RTSP] track(%d) recv error: %d\n", media, r);
		return -1;
	}
	//assert(0 == socket_addr_compare((const struct sockaddr*)&ss, (const struct sockaddr*)&ctx->ss[1]));

	if (media >= sizeof(ctx->demuxer) / sizeof(ctx->demuxer[0]) || ctx->demuxer[media] == NULL)
	{
		printf("[RTSP] track(%d) discard rtcp packet\n", media);
		return 0; // discard
	}
	r = rtsp_demuxer_input(ctx->demuxer[media], ctx->buffer, r);
	if (r < 0)
		return r;

	if (RTCP_BYE == r)
	{
		printf("finished\n");
	}

	// RTCP report
	r = rtsp_demuxer_rtcp(ctx->demuxer[media], ctx->buffer, sizeof(ctx->buffer));
	if (r > 0)
		r = socket_sendto(socket, ctx->buffer, r, 0, (const struct sockaddr*)&ss, len);
	return r < 0 ? r : 0;
}

static int rtsp_client_test2_ontcp(void* param, uint8_t channel, const void* data, uint16_t bytes)
{
	int r, media;
	struct rtsp_client_test2_t* ctx;
	ctx = (struct rtsp_client_test2_t*)param;

	media = channel / 2;
	if (media >= sizeof(ctx->demuxer) / sizeof(ctx->demuxer[0]) || ctx->demuxer[media] == NULL)
	{
		printf("[RTSP] track(%d) discard rtp packet\n", media);
		return 0; // discard
	}

	r = rtsp_demuxer_input(ctx->demuxer[media], data, bytes);
	return r < 0 ? r : 0;
}

static int rtsp_client_test2_send(void* param, const char* uri, const void* req, size_t bytes)
{
	//TODO: check uri and make socket
	//1. uri != rtsp describe uri(user input)
	//2. multi-uri if media_count > 1
	struct rtsp_client_test2_t* ctx = (struct rtsp_client_test2_t*)param;
	return socket_send_all_by_time(ctx->socket, req, bytes, 0, 2000);
}

static int rtsp_client_test2_rtpport(void* param, int media, const char* source, unsigned short rtp[2], char* ip, int len)
{
	int m, r;
	socket_t socket[2];
	struct rtsp_client_test2_t* ctx; 
	ctx  = (struct rtsp_client_test2_t*)param;

	m = rtsp_client_get_media_type(ctx->rtsp, media);
	if (SDP_M_MEDIA_AUDIO != m && SDP_M_MEDIA_VIDEO != m)
		return 0; // ignore

	switch (ctx->transport)
	{
	case RTSP_TRANSPORT_RTP_UDP:
		// TODO: ipv6
		r = sockpair_create("0.0.0.0", socket, rtp);
		if (0 != r)
			return r;

		// map fds to media index
		ctx->fds2media[ctx->fds_count] = media;
		ctx->fds2media[ctx->fds_count+1] = media;

		ctx->fds[ctx->fds_count].fd = socket[0];
		ctx->fds[ctx->fds_count].events = POLLIN;
		ctx->fds[ctx->fds_count].revents = 0;
		ctx->handlers[ctx->fds_count] = rtsp_client_test2_onrtp;
		ctx->fds_count++;

		ctx->fds[ctx->fds_count].fd = socket[1];
		ctx->fds[ctx->fds_count].events = POLLIN;
		ctx->fds[ctx->fds_count].revents = 0;
		ctx->handlers[ctx->fds_count] = rtsp_client_test2_onrtcp;
		ctx->fds_count++;

		socket_setrecvbuf(rtp[0], 512 * 1024);
		break;

	case RTSP_TRANSPORT_RTP_TCP:
		rtp[0] = 2 * media;
		rtp[1] = 2 * media + 1;
		break;

	default:
		assert(0);
		return -1;
	}

	return ctx->transport;
}

static int rtsp_client_test2_ondescribe(void* param, const char* sdp, int len)
{
	struct rtsp_client_test2_t* ctx = (struct rtsp_client_test2_t*)param;
	return rtsp_client_setup(ctx->rtsp, sdp, len);
}

static int rtsp_client_test2_onsetup(void* param, int timeout, int64_t duration)
{
	int i, j, r;
	uint64_t npt;
	struct rtsp_client_test2_t* ctx;
	const struct rtp_profile_t* profile;
	ctx = (struct rtsp_client_test2_t*)param;
	
	npt = 0;
	r = rtsp_client_play(ctx->rtsp, &npt, NULL);
	if (0 != r)
		return r;

	for (i = 0; i < rtsp_client_media_count(ctx->rtsp); i++)
	{
		int payload;
		const char* encoding;
		const struct rtsp_media_t* media;
		const struct rtsp_header_transport_t* transport;
		transport = rtsp_client_get_media_transport(ctx->rtsp, i);
		encoding = rtsp_client_get_media_encoding(ctx->rtsp, i);
		payload = rtsp_client_get_media_payload(ctx->rtsp, i);
		media = rtsp_client_get_media(ctx->rtsp, i);
		profile = rtp_profile_find(payload);
		if (RTSP_TRANSPORT_RTP_UDP == transport->transport)
		{
			//assert(RTSP_TRANSPORT_RTP_UDP == transport->transport); // udp only
			assert(0 == transport->multicast); // unicast only
			//assert(transport->rtp.u.client_port1 == ctx->port[i][0]);
			//assert(transport->rtp.u.client_port2 == ctx->port[i][1]);
			//port[0] = transport->rtp.u.server_port1;
			//port[1] = transport->rtp.u.server_port2;
			//rtp_receiver_test(ctx->rtp[i], *transport->source ? transport->source : ctx->ip, port, payload, encoding);

			//assert(0 == socket_addr_from(&ctx->ss[0], NULL, peer, (u_short)peerport[0]));
			//assert(0 == socket_addr_from(&ctx->ss[1], NULL, peer, (u_short)peerport[1]));
			//assert(0 == connect(rtp[0], (struct sockaddr*)&ctx->ss[0], len));
			//assert(0 == connect(rtp[1], (struct sockaddr*)&ctx->ss[1], len));
		}
		else if (RTSP_TRANSPORT_RTP_TCP == transport->transport)
		{
			//assert(transport->rtp.u.client_port1 == transport->interleaved1);
			//assert(transport->rtp.u.client_port2 == transport->interleaved2);
			//rtp_receiver_tcp_test(transport->interleaved1, transport->interleaved2, payload, encoding);
		}
		else
		{
			assert(0); // TODO
		}

		ctx->demuxer[i] = rtsp_demuxer_create(i, 500, rtsp_client_test2_onpacket, ctx);
		if (NULL == ctx->demuxer[i])
		{
			printf("[RTSP] rtsp_demuxer_create(%d, %s) error.\n", payload, encoding ? encoding : "");
			return -1; // ignore
		}

		for (j = 0; j < media->avformat_count; j++)
		{
			r = rtsp_demuxer_add_payload(ctx->demuxer[i], media->avformats[j].rate, media->avformats[j].fmt, media->avformats[j].encoding, media->avformats[j].fmtp);
			if (0 != r)
			{
				printf("RTSP] track[%d] add format [%d/%d/%s] failed.\n", i, media->avformats[j].rate, media->avformats[j].fmt, media->avformats[j].encoding);
				continue; // ignore
			}
		}
	}

	return 0;
}

static int rtsp_client_test2_onteardown(void* param)
{
	// todo
	return 0;
}

static int rtsp_client_test2_onplay(void* param, int media, const uint64_t* nptbegin, const uint64_t* nptend, const double* scale, const struct rtsp_rtp_info_t* rtpinfo, int count)
{
	// todo
	return 0;
}

static int rtsp_client_test2_onpause(void* param)
{
	// todo
	return 0;
}

static int rtsp_client_test2_create(struct rtsp_client_test2_t *ctx, const char* url, const char* username, const char* password, int transport)
{
	struct rtsp_client_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = rtsp_client_test2_send;
	handler.rtpport = rtsp_client_test2_rtpport;
	handler.ondescribe = rtsp_client_test2_ondescribe;
	handler.onsetup = rtsp_client_test2_onsetup;
	handler.onplay = rtsp_client_test2_onplay;
	handler.onpause = rtsp_client_test2_onpause;
	handler.onteardown = rtsp_client_test2_onteardown;
	handler.onrtp = rtsp_client_test2_ontcp;

	ctx->rtsp = rtsp_client_create(url, username, password, &handler, ctx);
	ctx->transport = 1 == transport ? RTSP_TRANSPORT_RTP_TCP : RTSP_TRANSPORT_RTP_UDP;
	return 0;
}

static int rtsp_client_test2_destroy(struct rtsp_client_test2_t* ctx)
{
	if (ctx->rtsp)
		rtsp_client_destroy(ctx->rtsp);
	ctx->rtsp = NULL;

	avpkt2bs_destroy(&ctx->bs);
	return 0;
}

static int rtsp_client_test2_start(struct rtsp_client_test2_t* ctx, const char* url)
{
	int r;
	socket_t socket;
	struct uri_t* uri;

	uri = uri_parse(url, strlen(url));
	if (!uri || !uri->host || !uri->scheme)
	{
		printf("[RTSP] uri [%s] parse error.\n", url);
		return -1;
	}

	if (0 == uri->port)
		uri->port = PORT_RTSP;

	socket = socket_connect_host(uri->host, uri->port, 2000);

	uri_free(uri);
	if (socket_invalid == socket)
		return -1;

	ctx->socket = socket;
	socket_setnonblock(socket, 1);
	r = rtsp_client_describe(ctx->rtsp);
	if (0 != r)
	{
		socket_close(socket);
		return r;
	}

	ctx->fds_count = 1;
	ctx->fds[0].fd = socket;
	ctx->fds[0].events = POLLIN;
	ctx->fds[0].revents = 0;
	ctx->fds2media[0] = -1;
	ctx->handlers[0] = rtsp_client_test2_onrtsp;

	avpkt2bs_create(&ctx->bs);

	r = rtsp_client_test2_run(ctx);
	printf("[RTSP] exit with: %d\n", r);

	socket_setnonblock(socket, 0);
	r = rtsp_client_teardown(ctx->rtsp);
	rtsp_client_destroy(ctx->rtsp);
	socket_close(socket);
	return 0;
}

void rtsp_client_test2(const char* url, const char* username, const char* password)
{
	struct rtsp_client_test2_t ctx;

	socket_init(); // optional

	memset(&ctx, 0, sizeof(ctx));
	rtsp_client_test2_create(&ctx, url, username, password, 1 /*1-tcp, 0-udp*/);
	rtsp_client_test2_start(&ctx, url);
	rtsp_client_test2_destroy(&ctx);

	socket_cleanup(); // optional
}
