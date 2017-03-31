#include "rtmp-client.h"
#include "rtmp-internal.h"
#include "rtmp-msgtypeid.h"
#include "rtmp-handshake.h"
#include "rtmp-control-message.h"
#include "rtmp-netconnection.h"
#include "rtmp-netstream.h"
#include "rtmp-event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#define strlcpy(t, s, n) strcpy_s(t, n, s)
#endif

#define N_URL 1024
#define N_STREAM 2
#define VIDEO_STREAM 0
#define AUDIO_STREAM 1

#define RTMP_META_LEN (1024*8)

enum
{
	RTMP_HANDSHAKE_S0, // client wait S0
	RTMP_HANDSHAKE_S1, // client received S0, wait S1
	RTMP_HANDSHAKE_S2, // client received S1, wait S2
	RTMP_HANDSHAKE_DONE, // client received S2
};

// Chunk Stram Id
enum
{
	RTMP_CHANNEL_CONTROL = 3,
	RTMP_CHANNEL_AUDIO,
	RTMP_CHANNEL_VIDEO,
};

struct rtmp_client_t
{
	struct rtmp_t rtmp;

	struct rtmp_client_handler_t handler;
	void* param;

	struct rtmp_connect_t connect;

	uint8_t payload[2*1024];
	//uint8_t handshake[RTMP_HANDSHAKE + 1]; // only for handshake
	size_t handshake_bytes;
	int handshake_state; // 0-init, 1-received S0, 2-received S1, 3-received S2

	int vod; // 0-publish, 1-vod

	void* streams[N_STREAM];
	size_t stream_bytes[N_STREAM];
	size_t capacity[N_STREAM];

	int status;
};

static void rtmp_client_chunk_header_default(struct rtmp_chunk_header_t* header, uint32_t channel, uint32_t timestamp, uint32_t length, uint8_t type, uint32_t stream_id)
{
	header->fmt = RTMP_CHUNK_TYPE_0; // disable compact header
	header->cid = channel;
	header->timestamp = timestamp;
	header->length = length;
	header->type = type;
	header->stream_id = stream_id; /* default 0 */
}

// C2
static void rtmp_client_send_c2(struct rtmp_client_t* ctx)
{
	int r;
	assert(RTMP_HANDSHAKE_S2 == ctx->handshake_state);
	rtmp_handshake_c2(ctx->payload, (uint32_t)time(NULL), ctx->payload, RTMP_HANDSHAKE);
	r = ctx->handler.send(ctx->param, ctx->payload, RTMP_HANDSHAKE);
	if (RTMP_HANDSHAKE != r)
		ctx->handler.onerror(ctx->param, -1, "error: send C2");
}

// Connect
static void rtmp_client_send_connect(struct rtmp_client_t* ctx)
{
	int r, i;
	struct rtmp_chunk_header_t header;
	assert(0 == ctx->rtmp.transaction_id);
	r = rtmp_netconnection_connect(ctx->payload, sizeof(ctx->payload), ++ctx->rtmp.transaction_id, &ctx->connect) - ctx->payload;
	i = rtmp_command_transaction_save(&ctx->rtmp, ctx->rtmp.transaction_id, RTMP_NETCONNECTION_CONNECT);
	rtmp_client_chunk_header_default(&header, RTMP_CHANNEL_CONTROL, 0, r, RTMP_TYPE_INVOKE, 0);
	r = rtmp_message_send(&ctx->rtmp, &header, ctx->payload);
	if (0 != r)
	{
		memset(&ctx->rtmp.transactions[i], 0, sizeof(struct rtmp_transaction_t));
		ctx->handler.onerror(ctx->param, r, "error: send command: connect");
	}
}

// ReleaseStream
static void rmtp_client_send_release_stream(struct rtmp_client_t* ctx)
{
	int r;
	struct rtmp_chunk_header_t header;
	r = rtmp_netstream_release_stream(ctx->payload, sizeof(ctx->payload), 0, ctx->rtmp.playpath) - ctx->payload;
	rtmp_client_chunk_header_default(&header, RTMP_CHANNEL_CONTROL, 0, r, RTMP_TYPE_INVOKE, (uint32_t)ctx->rtmp.stream_id);
	r = rtmp_message_send(&ctx->rtmp, &header, ctx->payload);
	if (0 != r)
		ctx->handler.onerror(ctx->param, r, "error: send command: releaseStream");
}

// FCPublish
static void rtmp_client_send_fcpublish(struct rtmp_client_t* ctx)
{
	int r;
	struct rtmp_chunk_header_t header;
	r = rtmp_netstream_fcpublish(ctx->payload, sizeof(ctx->payload), 0, ctx->rtmp.playpath) - ctx->payload;
	rtmp_client_chunk_header_default(&header, RTMP_CHANNEL_CONTROL, 0, r, RTMP_TYPE_INVOKE, 0);
	r = rtmp_message_send(&ctx->rtmp, &header, ctx->payload);
	if (0 != r)
		ctx->handler.onerror(ctx->param, r, "error: send command: FCPublish");
}

// FCUnpublish
static void rtmp_client_send_fcunpublish(struct rtmp_client_t* ctx)
{
	int r;
	struct rtmp_chunk_header_t header;
	r = rtmp_netstream_fcunpublish(ctx->payload, sizeof(ctx->payload), 0, ctx->rtmp.playpath) - ctx->payload;
	rtmp_client_chunk_header_default(&header, RTMP_CHANNEL_CONTROL, 0, r, RTMP_TYPE_INVOKE, (uint32_t)ctx->rtmp.stream_id);
	r = rtmp_message_send(&ctx->rtmp, &header, ctx->payload);
	if (0 != r)
		ctx->handler.onerror(ctx->param, r, "error: send command: FCUnpublish");
}

// createStream
static void rtmp_client_send_create_stream(struct rtmp_client_t* ctx)
{
	int r, i;
	struct rtmp_chunk_header_t header;
	r = rtmp_netconnection_create_stream(ctx->payload, sizeof(ctx->payload), ++ctx->rtmp.transaction_id) - ctx->payload;
	i = rtmp_command_transaction_save(&ctx->rtmp, ctx->rtmp.transaction_id, RTMP_NETCONNECTION_CREATE_STREAM);
	rtmp_client_chunk_header_default(&header, RTMP_CHANNEL_CONTROL, 0, r, RTMP_TYPE_INVOKE, 0);
	r = rtmp_message_send(&ctx->rtmp, &header, ctx->payload);
	if (0 != r)
	{
		memset(&ctx->rtmp.transactions[i], 0, sizeof(struct rtmp_transaction_t));
		ctx->handler.onerror(ctx->param, r, "error: send command: createStream");
	}
}

// deleteStream
static void rtmp_client_send_delete_stream(struct rtmp_client_t* ctx)
{
	int r;
	struct rtmp_chunk_header_t header;
	assert(0 != ctx->rtmp.stream_id);
	r = rtmp_netstream_delete_stream(ctx->payload, sizeof(ctx->payload), 0, (uint32_t)ctx->rtmp.stream_id) - ctx->payload;
	rtmp_client_chunk_header_default(&header, RTMP_CHANNEL_CONTROL, 0, r, RTMP_TYPE_INVOKE, (uint32_t)ctx->rtmp.stream_id);
	r = rtmp_message_send(&ctx->rtmp, &header, ctx->payload);
	if (0 != r)
	{
		ctx->handler.onerror(ctx->param, r, "error: send command: deleteStream");
	}
}

// publish
static void rtmp_client_send_publish(struct rtmp_client_t* ctx)
{
	int r;
	struct rtmp_chunk_header_t header;
	assert(0 != ctx->rtmp.stream_id);
	r = rtmp_netstream_publish(ctx->payload, sizeof(ctx->payload), 0, ctx->rtmp.playpath, RTMP_STREAM_LIVE) - ctx->payload;
	rtmp_client_chunk_header_default(&header, RTMP_CHANNEL_CONTROL, 0, r, RTMP_TYPE_INVOKE, (uint32_t)ctx->rtmp.stream_id);
	r = rtmp_message_send(&ctx->rtmp, &header, ctx->payload);
	if (0 != r)
	{
		ctx->handler.onerror(ctx->param, r, "error: send command: publish");
	}
}

// play
static void rtmp_client_send_play(struct rtmp_client_t* ctx)
{
	int r;
	struct rtmp_chunk_header_t header;
	assert(0 != ctx->rtmp.stream_id);
	// TODO: time
	r = rtmp_netstream_play(ctx->payload, sizeof(ctx->payload), 0, ctx->rtmp.playpath, -2, -1, 1) - ctx->payload;
	rtmp_client_chunk_header_default(&header, RTMP_CHANNEL_CONTROL, (uint32_t)time(NULL), r, RTMP_TYPE_INVOKE, (uint32_t)ctx->rtmp.stream_id);
	r = rtmp_message_send(&ctx->rtmp, &header, ctx->payload);
	if (0 != r)
	{
		ctx->handler.onerror(ctx->param, r, "error: send command: paly");
	}
}

// Window Acknowledgement Size (5)
static void rtmp_client_send_server_bandwidth(struct rtmp_client_t* ctx)
{
	int n, r;
	n = rtmp_window_acknowledgement_size(ctx->payload, sizeof(ctx->payload), ctx->rtmp.window_size);
	r = ctx->handler.send(ctx->param, ctx->payload, n);
	if (n != r)
		ctx->handler.onerror(ctx->param, r, "error: send command: bandwidth");
}

static void rtmp_client_send_set_buffer_length(struct rtmp_client_t* ctx)
{
	int n, r;
	n = rtmp_event_set_buffer_length(ctx->payload, sizeof(ctx->payload), (uint32_t)ctx->rtmp.stream_id, ctx->rtmp.buffer_length_ms);
	r = ctx->handler.send(ctx->param, ctx->payload, n);
	if (n != r)
		ctx->handler.onerror(ctx->param, r, "error: send command: SetBufferLength");
}

static void rtmp_client_onconnect(void* param)
{
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)param;
	if (0 == ctx->vod)
	{
		// publish only
		rmtp_client_send_release_stream(ctx);
		rtmp_client_send_fcpublish(ctx);
	}
	else
	{
		rtmp_client_send_server_bandwidth(ctx);
		rtmp_client_send_set_buffer_length(ctx);
	}
	rtmp_client_send_create_stream(ctx);
}

static void rtmp_client_oncreate_stream(void* param, uint32_t stream_id)
{
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)param;
	if (0 == ctx->vod)
	{
		rtmp_client_send_publish(ctx);
	}
	else
	{
		rtmp_client_send_play(ctx);
		rtmp_client_send_set_buffer_length(ctx);
	}
}

static void rtmp_client_onnotify(void* param, enum rtmp_notify_t notify)
{
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)param;
	switch (notify)
	{
	case RTMP_NOTIFY_START:
		ctx->status = RTMP_NOTIFY_START;
		printf("rtmp start video on demand\n");
		break;
	case RTMP_NOTIFY_STOP:
		ctx->status = RTMP_NOTIFY_STOP;
		break;
	case RTMP_NOTIFY_PAUSE:
		ctx->status = RTMP_NOTIFY_PAUSE;
		break;
	case RTMP_NOTIFY_SEEK:
		break;
	default:
		break;
	}
}

static void rtmp_client_onping(void* param, uint32_t seqNo)
{
	int n, r;
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)param;

	n = rtmp_event_pong(ctx->payload, sizeof(ctx->payload), seqNo);
	r = ctx->handler.send(ctx->param, ctx->payload, n);
	if (n != r)
		ctx->handler.onerror(ctx->param, r, "error: send command: PingResponse");
}

static void rtmp_client_onbandwidth(void* param)
{
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)param;
	rtmp_client_send_server_bandwidth(ctx);
}

static void rtmp_client_onerror(void* param, int code, const char* msg)
{
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)param;
	ctx->handler.onerror(ctx->param, code, msg);
}

static void rtmp_client_onaudio(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)param;
	ctx->handler.onaudio(ctx->param, data, bytes, timestamp);
}

static void rtmp_client_onvideo(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)param;
	ctx->handler.onvideo(ctx->param, data, bytes, timestamp);
}

static uint8_t s_payload[1024*2];
static int rtmp_client_send(void* param, const uint8_t* header, uint32_t headerBytes, const uint8_t* payload, uint32_t payloadBytes)
{
	int r;
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)param;
	//memcpy(s_payload, header, headerBytes);
	//memcpy(s_payload + headerBytes, payload, payloadBytes);
	//r = ctx->handler.send(ctx->param, s_payload, headerBytes + payloadBytes);
	r = ctx->handler.send(ctx->param, header, headerBytes);
	if (r >= 0 && payloadBytes > 0)
		r = ctx->handler.send(ctx->param, payload, payloadBytes);
	return r;
}

void* rtmp_client_create(const char* appname, const char* playpath, const char* tcurl, void* param, const struct rtmp_client_handler_t* handler)
{
	struct rtmp_client_t* ctx;

	assert(appname && *appname && playpath && *playpath && handler);
	ctx = (struct rtmp_client_t*)malloc(sizeof(struct rtmp_client_t));
	if (!ctx) return NULL;

	memset(ctx, 0, sizeof(struct rtmp_client_t));
	memcpy(&ctx->handler, handler, sizeof(ctx->handler));
	ctx->param = param;

	ctx->rtmp.parser.state = RTMP_STATE_INIT;
	ctx->rtmp.in_chunk_size = RTMP_CHUNK_SIZE;
	ctx->rtmp.out_chunk_size = RTMP_CHUNK_SIZE;
	ctx->rtmp.window_size = 2500000;
	ctx->rtmp.peer_bandwidth = 2500000;
	ctx->rtmp.buffer_length_ms = 30000;
	strlcpy(ctx->rtmp.playpath, playpath, sizeof(ctx->rtmp.playpath));

	ctx->rtmp.param = ctx;
	ctx->rtmp.send = rtmp_client_send;
	ctx->rtmp.onerror = rtmp_client_onerror;
	ctx->rtmp.u.client.onconnect = rtmp_client_onconnect;
	ctx->rtmp.u.client.oncreate_stream = rtmp_client_oncreate_stream;
	ctx->rtmp.u.client.onnotify = rtmp_client_onnotify;
	ctx->rtmp.u.client.onping = rtmp_client_onping;
	ctx->rtmp.u.client.onbandwidth = rtmp_client_onbandwidth;
	ctx->rtmp.u.client.onaudio = rtmp_client_onaudio;
	ctx->rtmp.u.client.onvideo = rtmp_client_onvideo;

	strlcpy(ctx->connect.app, appname, sizeof(ctx->connect.app));
	if (tcurl) strlcpy(ctx->connect.tcUrl, tcurl, sizeof(ctx->connect.tcUrl));
	//strlcpy(ctx->connect.swfUrl, tcurl ? tcurl : url, sizeof(ctx->connect.swfUrl));
	//strlcpy(ctx->connect.pageUrl, tcurl ? tcurl : url, sizeof(ctx->connect.pageUrl));
	strlcpy(ctx->connect.flashver, "LNX 9,0,124,2", sizeof(ctx->connect.flashver));
	ctx->connect.fpad = 0;
	ctx->connect.capabilities = 15;
	ctx->connect.audioCodecs = 3191; //SUPPORT_SND_AAC;
	ctx->connect.videoCodecs = 252; // SUPPORT_VID_H264;
	ctx->connect.videoFunction = SUPPORT_VID_CLIENT_SEEK;

	return ctx;
}

void rtmp_client_destroy(void* client)
{
	size_t i;
	struct rtmp_client_t* ctx;
	if (NULL == client)
		return;
	ctx = (struct rtmp_client_t*)client;

	if (ctx->streams[AUDIO_STREAM])
		free(ctx->streams[AUDIO_STREAM]);
	if (ctx->streams[VIDEO_STREAM])
		free(ctx->streams[VIDEO_STREAM]);

	for (i = 0; i < N_CHUNK_STREAM; i++)
	{
		if (ctx->rtmp.in_packets->payload)
			free(ctx->rtmp.in_packets->payload);
	}

	free(ctx);
}

int rtmp_client_input(void* client, const void* data, size_t bytes)
{
	const uint8_t* p;
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)client;

	p = data;
	while (bytes > 0)
	{
		switch (ctx->handshake_state)
		{
		case RTMP_HANDSHAKE_S0: // S0: version
			ctx->handshake_state = RTMP_HANDSHAKE_S1;
			ctx->handshake_bytes = 0; // clear buffer
			assert(*p <= RTMP_VERSION);
			bytes -= 1;
			p += 1;
			break;

		case RTMP_HANDSHAKE_S1: // S1: 4-time + 4-zero + 1528-random
			if (bytes + ctx->handshake_bytes < RTMP_HANDSHAKE)
			{
				memcpy(ctx->payload + ctx->handshake_bytes, p, bytes);
				ctx->handshake_bytes += bytes;
				p += bytes;
				bytes = 0; // 0
			}
			else
			{
				memcpy(ctx->payload + ctx->handshake_bytes, p, RTMP_HANDSHAKE - ctx->handshake_bytes);
				bytes -= RTMP_HANDSHAKE - ctx->handshake_bytes;
				p += RTMP_HANDSHAKE - ctx->handshake_bytes;
				ctx->handshake_state = RTMP_HANDSHAKE_S2;
				ctx->handshake_bytes = 0; // clear buffer
				rtmp_client_send_c2(ctx);
			}
			break;

		case RTMP_HANDSHAKE_S2: // S2: 4-time + 4-time2 + 1528-echo
			if (bytes + ctx->handshake_bytes < RTMP_HANDSHAKE)
			{
				memcpy(ctx->payload + ctx->handshake_bytes, p, bytes);
				ctx->handshake_bytes += bytes;
				p += bytes;
				bytes = 0; // 0
			}
			else
			{
				memcpy(ctx->payload + ctx->handshake_bytes, p, RTMP_HANDSHAKE - ctx->handshake_bytes);
				bytes -= RTMP_HANDSHAKE - ctx->handshake_bytes;
				p += RTMP_HANDSHAKE - ctx->handshake_bytes;
				ctx->handshake_state = RTMP_HANDSHAKE_DONE;
				ctx->handshake_bytes = 0; // clear buffer
				rtmp_client_send_connect(ctx);
			}
			break;

		case RTMP_HANDSHAKE_DONE:
		default:
			return rtmp_chunk_input(&ctx->rtmp, (const uint8_t*)data, bytes);
		}
	}

	return 0;
}

int rtmp_client_start(void* client, int vod)
{
	int n;
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)client;
	ctx->rtmp.transaction_id = 0;
	ctx->vod = vod;

	// handshake C0/C1
	ctx->handshake_state = RTMP_HANDSHAKE_S0;
	n = rtmp_handshake_c0(ctx->payload, RTMP_VERSION);
	n += rtmp_handshake_c1(ctx->payload + n, (uint32_t)time(NULL));
	assert(n == RTMP_HANDSHAKE + 1);
	return ctx->handler.send(ctx->param, ctx->payload, n);
}

int rtmp_client_stop(void* client)
{
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)client;

	if (0 == ctx->vod)
	{
		rtmp_client_send_fcunpublish(ctx);
	}

	rtmp_client_send_delete_stream(ctx);
	return 0;
}

int rtmp_client_pause(void* client, int pause)
{
	int r, i;
	uint32_t timestamp = 0;
	struct rtmp_client_t* ctx;
	struct rtmp_chunk_header_t header;
	ctx = (struct rtmp_client_t*)client;

	for (i = 0; i < N_CHUNK_STREAM; i++)
	{
		if(0 == ctx->rtmp.in_packets[i].header.cid)
			continue;
		if (timestamp < ctx->rtmp.in_packets[i].header.timestamp)
			timestamp = ctx->rtmp.in_packets[i].header.timestamp;
	}

	r = rtmp_netstream_pause(ctx->payload, sizeof(ctx->payload), 0, pause, timestamp) - ctx->payload;
	rtmp_client_chunk_header_default(&header, RTMP_CHANNEL_CONTROL, 0, r, RTMP_TYPE_INVOKE, (uint32_t)ctx->rtmp.stream_id);
	r = rtmp_message_send(&ctx->rtmp, &header, ctx->payload);
	if (0 != r)
		ctx->handler.onerror(ctx->param, r, "error: send command: pause");
	return r;
}

int rtmp_client_seek(void* client, double timestamp)
{
	int r;
	struct rtmp_client_t* ctx;
	struct rtmp_chunk_header_t header;
	ctx = (struct rtmp_client_t*)client;

	r = rtmp_netstream_seek(ctx->payload, sizeof(ctx->payload), 0, timestamp) - ctx->payload;
	rtmp_client_chunk_header_default(&header, RTMP_CHANNEL_CONTROL, 0, r, RTMP_TYPE_INVOKE, (uint32_t)ctx->rtmp.stream_id);
	r = rtmp_message_send(&ctx->rtmp, &header, ctx->payload);
	if (0 != r)
		ctx->handler.onerror(ctx->param, r, "error: send command: seek");

	return r;
}

int rtmp_client_getstatus(void* client)
{
	struct rtmp_client_t* ctx;
	ctx = (struct rtmp_client_t*)client;
	return ctx->status;
}

int rtmp_client_push_video(void* client, const void* video, size_t bytes, uint32_t timestamp)
{
	struct rtmp_client_t* ctx;
	struct rtmp_chunk_header_t header;
	ctx = (struct rtmp_client_t*)client;

	assert(0 != ctx->rtmp.stream_id);
	header.fmt = RTMP_CHUNK_TYPE_1; // enable compact header
	header.cid = RTMP_CHANNEL_VIDEO;
	header.timestamp = timestamp;
	header.length = (uint32_t)bytes;
	header.type = RTMP_TYPE_VIDEO;
	header.stream_id = (uint32_t)ctx->rtmp.stream_id;

	return rtmp_message_send(&ctx->rtmp, &header, (const uint8_t*)video);
}

int rtmp_client_push_audio(void* client, const void* audio, size_t bytes, uint32_t timestamp)
{
	struct rtmp_client_t* ctx;
	struct rtmp_chunk_header_t header;
	ctx = (struct rtmp_client_t*)client;

	assert(0 != ctx->rtmp.stream_id);
	header.fmt = RTMP_CHUNK_TYPE_1; // enable compact header
	header.cid = RTMP_CHANNEL_AUDIO;
	header.timestamp = timestamp;
	header.length = (uint32_t)bytes;
	header.type = RTMP_TYPE_AUDIO;
	header.stream_id = (uint32_t)ctx->rtmp.stream_id;

	return rtmp_message_send(&ctx->rtmp, &header, (const uint8_t*)audio);
}
