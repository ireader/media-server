#include <stdint.h>
#include "sockutil.h"
#include "sys/thread.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include "rtp.h"
#include "time64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct rtp_context_t
{
	FILE *fp;
	FILE *frtp;

	char encoding[64];
	u_short port[2];
	socket_t socket[2];
	struct sockaddr_storage ss;
	socklen_t len;

	char rtp_buffer[64 * 1024];
	char rtcp_buffer[32 * 1024];

	void* payload;
	void* rtp;
};

static int rtp_read(struct rtp_context_t* ctx, socket_t s)
{
	int r;
	uint8_t size[2];
	static int i, n = 0;
	socklen_t len;
	struct sockaddr_storage ss;
	len = sizeof(ss);

	r = recvfrom(s, ctx->rtp_buffer, sizeof(ctx->rtp_buffer), 0, (struct sockaddr*)&ss, &len);
	if (r < 12)
		return -1;
	assert(AF_INET == ss.ss_family);
	assert(((struct sockaddr_in*)&ss)->sin_port == htons(ctx->port[0]));
	assert(0 == memcmp(&((struct sockaddr_in*)&ss)->sin_addr, &((struct sockaddr_in*)&ctx->ss)->sin_addr, 4));

	n += r;
	if(0 == i++ % 100)
		printf("packet: %d, seq: %u, size: %d/%d\n", i, ((uint8_t)ctx->rtp_buffer[2] << 8) | (uint8_t)ctx->rtp_buffer[3], r, n);
	
	size[0] = r >> 8;
	size[1] = r >> 0;
	fwrite(size, 1, sizeof(size), ctx->frtp);
	fwrite(ctx->rtp_buffer, 1, r, ctx->frtp);

	rtp_payload_decode_input(ctx->payload, ctx->rtp_buffer, r);
	rtp_onreceived(ctx->rtp, ctx->rtp_buffer, r);
	return r;
}

static int rtcp_read(struct rtp_context_t* ctx, socket_t s)
{
	int r;
	socklen_t len;
	struct sockaddr_storage ss;
	len = sizeof(ss);
	r = recvfrom(s, ctx->rtcp_buffer, sizeof(ctx->rtcp_buffer), 0, (struct sockaddr*)&ss, &len);
	if (r < 12)
		return -1;
	assert(AF_INET == ss.ss_family);
	assert(((struct sockaddr_in*)&ss)->sin_port == htons(ctx->port[1]));
	assert(0 == memcmp(&((struct sockaddr_in*)&ss)->sin_addr, &((struct sockaddr_in*)&ctx->ss)->sin_addr, 4));

	r = rtp_onreceived_rtcp(ctx->rtp, ctx->rtcp_buffer, r);
	fflush(ctx->fp);
	return r;
}

#if defined(OS_WINDOWS)
static int rtp_receiver(struct rtp_context_t* ctx, socket_t rtp[2], int timeout)
{
	int r;
	int interval;
	time64_t clock;
	fd_set fds;
	struct timeval tv;

	clock = time64_now();
	while (1)
	{
		FD_ZERO(&fds);
		FD_SET(rtp[0], &fds);
		FD_SET(rtp[1], &fds);

		// RTCP report
		interval = rtp_rtcp_interval(ctx->rtp);
		if (clock + interval < time64_now())
		{
			r = rtp_rtcp_report(ctx->rtp, ctx->rtcp_buffer, sizeof(ctx->rtcp_buffer));
			r = sendto(rtp[1], ctx->rtcp_buffer, r, 0, (struct sockaddr*)&ctx->ss, ctx->len);
			clock = time64_now();
		}

		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		r = select(0, &fds, NULL, NULL, timeout < 0 ? NULL : &tv);
		if (0 == r)
		{
			continue; // timeout
		}
		else if (r < 0)
		{
			return r; // error
		}
		else
		{
			if (FD_ISSET(rtp[0], &fds))
			{
				rtp_read(ctx, rtp[0]);
			}

			if (FD_ISSET(rtp[1], &fds))
			{
				rtcp_read(ctx, rtp[1]);
			}
		}
	}
}
#else
static int rtp_receiver(struct rtp_context_t* ctx, socket_t rtp[2], int timeout)
{
	int i, r;
	int interval;
	time64_t clock;
	struct pollfd fds[2];

	for (i = 0; i < 2; i++)
	{
		fds[i].fd = rtp[i];
		fds[i].events = POLLIN;
		fds[i].revents = 0;
	}

	clock = time64_now();
	while (1)
	{
		// RTCP report
		interval = rtp_rtcp_interval(ctx->rtp);
		if (clock + interval < time64_now())
		{
			r = rtp_rtcp_report(ctx->rtp, ctx->rtcp_buffer, sizeof(ctx->rtcp_buffer));
			r = socket_send_all_by_time(rtp[1], ctx->rtcp_buffer, r, 0, timeout);
			clock = time64_now();
		}

		r = poll(&fds, 2, timeout);
		while (-1 == r && EINTR == errno)
			r = poll(&fds, 1, timeout);

		if (0 == r)
		{
			continue; // timeout
		}
		else if (r < 0)
		{
			return r; // error
		}
		else
		{
			if (0 != fds[0].revents)
			{
				rtp_read(ctx, rtp[0]);
				fds[0].revents = 0;
			}

			if (0 != fds[0].revents)
			{
				rtcp_read(ctx, rtp[1]);
				fds[1].revents = 0;
			}
		}
	}
	return r;
}
#endif

static void rtp_packet(void* param, const void *packet, int bytes, int64_t time, int flags)
{
	const uint8_t start_code[] = { 0, 0, 0, 1 };
	struct rtp_context_t* ctx;
	ctx = (struct rtp_context_t*)param;
	if(0 == strcmp("H264", ctx->encoding) || 0 == strcmp("H265", ctx->encoding))
		fwrite(start_code, 1, 4, ctx->fp);
	fwrite(packet, 1, bytes, ctx->fp);
	(void)time;
	(void)flags;
}

static void rtp_on_rtcp(void* param, const struct rtcp_msg_t* msg)
{
	struct rtp_context_t* ctx;
	ctx = (struct rtp_context_t*)param;
	if (RTCP_MSG_BYE == msg->type)
	{
		printf("finished\n");
	}
}

static int STDCALL rtp_worker(void* param)
{
	struct rtp_context_t* ctx;
	ctx = (struct rtp_context_t*)param;

	rtp_receiver(ctx, ctx->socket, 2000);

	rtp_destroy(ctx->rtp);
	rtp_payload_decode_destroy(ctx->payload);
	fclose(ctx->frtp);
	fclose(ctx->fp);
	free(ctx);
	return 0;
}

void rtp_receiver_test(socket_t rtp[2], const char* peer, int peerport[2], int payload, const char* encoding)
{
	size_t n;
	pthread_t t;
	struct rtp_context_t* ctx;
	struct rtp_event_t evthandler;
	struct rtp_payload_t handler;
	const struct rtp_profile_t* profile;

	ctx = malloc(sizeof(*ctx));
	snprintf(ctx->rtp_buffer, sizeof(ctx->rtp_buffer), "%s.%d.%d.%s", peer, peerport[0], payload, encoding);
	snprintf(ctx->rtcp_buffer, sizeof(ctx->rtcp_buffer), "%s.%d.%d.%s.rtp", peer, peerport[0], payload, encoding);
	ctx->fp = fopen(ctx->rtp_buffer, "wb");
	ctx->frtp = fopen(ctx->rtcp_buffer, "wb");

	socket_getrecvbuf(rtp[0], &n);
	socket_setrecvbuf(rtp[0], 256*1024);
	socket_getrecvbuf(rtp[0], &n);

	profile = rtp_profile_find(payload);
	
	handler.alloc = NULL;
	handler.free = NULL;
	handler.packet = rtp_packet;
	ctx->payload = rtp_payload_decode_create(payload, encoding, &handler, ctx);
	
	evthandler.on_rtcp = rtp_on_rtcp;
	ctx->rtp = rtp_create(&evthandler, &ctx, (uint32_t)(intptr_t)&ctx, profile ? profile->frequency : 9000, 2*1024*1024);

	assert(0 == socket_addr_from(&ctx->ss, &ctx->len, peer, (u_short)peerport[0]));
	//assert(0 == socket_addr_setport((struct sockaddr*)&ss, len, (u_short)peerport[0]));
	//assert(0 == connect(rtp[0], (struct sockaddr*)&ss, len));
	assert(0 == socket_addr_setport((struct sockaddr*)&ctx->ss, ctx->len, (u_short)peerport[1]));
	//assert(0 == connect(rtp[1], (struct sockaddr*)&ss, len));

	snprintf(ctx->encoding, sizeof(ctx->encoding), "%s", encoding);
	ctx->socket[0] = rtp[0];
	ctx->socket[1] = rtp[1];
	ctx->port[0] = (u_short)peerport[0];
	ctx->port[1] = (u_short)peerport[1];
	if (0 == thread_create(&t, rtp_worker, ctx))
		thread_detach(t);
}
