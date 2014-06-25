#include "hls-server.h"
#include "http-server.h"
#include "hls-live.h"
#include "hls-file.h"
#include "urlcodec.h"
#include "sys/system.h"
#include <stdlib.h>
#include <memory.h>

struct hls_server_t
{
	void* http;

	hls_live_open open;
	hls_live_close close;
	void* param;
};

static int hls_server_reply(void* session, int code, const char* msg)
{
	void* bundle;
	void* ptr;
	bundle = http_bundle_alloc(strlen(msg)+1);
	ptr = http_bundle_lock(bundle);
	strcpy((char*)ptr, msg);
	http_bundle_unlock(bundle, strlen(msg)+1);
	http_server_send(session, code, bundle);
	http_bundle_free(bundle);
	return 0;
}

// /live/name.m3u8
static int hls_server_live_m3u8(struct hls_server_t *ctx, void* session, const char* path)
{
	int i, r, n;
	char name[64];
	char server[64];
	char key[64];
	//char m3u8[512];
	char *m3u8;
	void *content;
	time64_t ltnow;
	void* bundle;
	struct hls_live_t *live;

	n = strlen(path);
	if(n - 6 - 5 >= sizeof(name))
	{
		return hls_server_reply(session, 404, "");
	}
	else
	{
		strncpy(name, path + 6, n - 6 - 5);
		name[n - 6 - 5] = '\0';
	}

	n = 0;
	http_server_get_content(session, &content, &n);
	if(n < 0)
		return hls_server_reply(session, 417, "");

	// parse server/key
	live = hls_live_fetch(ctx, name);
	if(!live)
		return hls_server_reply(session, 500, "");

	// create a new thread?
	if(ctx->open)
	{
		r = ctx->open(ctx->param, live, name, server, key);
		if(0 != r)
		{
		}
	}

	// wait for first file
	ltnow = time64_now();
	while(live->file_count < 1 && time64_now() - ltnow < MAX_DURATION*2000)
	{
		system_sleep(200);
	}

	bundle = http_bundle_alloc(128*MAX_FILES);
	m3u8 = http_bundle_lock(bundle);
	if(0 != hls_live_m3u8(live, m3u8))
	{
		http_bundle_free(bundle);
		return hls_server_reply(session, 500, "");
	}

	http_bundle_unlock(bundle, strlen(m3u8));
	http_server_set_content_type(session, "application/vnd.apple.mpegurl");
	http_server_send(session, 200, bundle);
	http_bundle_free(bundle);
	return 0;
}

static hls_server_live(struct hls_server_t *ctx, void* session, const char* path)
{
	int i, n;
	char name[64];
	char file[64];
	const char* p;
	void** bundles;
	struct hls_live_t *live;
	struct hls_file_t *tsfile;
	struct hls_block_t *block;

	n = strlen(path);
	assert(n < sizeof(name) + sizeof(file) + 6);

	// /live/name/1.ts
	p = strrchr(path, '/');
	if(n - (p - path) > sizeof(file) || p - path - 6 > sizeof(name) || p - path < 7)
	{
		return hls_server_reply(session, 404, "");
	}
	else
	{
		strncpy(name, path + 6, p - path - 6);
		strncpy(file, p + 1, n - (p + 1 - path) - 3);
		name[p - path - 6] = '\0';
		file[n - (p + 1 - path) - 3] = '\0';

		live = hls_live_fetch(ctx, name);
		if(!live)
			return hls_server_reply(session, 500, "");

		tsfile = hls_live_read(live, file);
		if(!tsfile)
			return hls_server_reply(session, 404, "");

		i = 0;
		n = 0;
		for(block = &tsfile->head; block; block = block->next)
			++n;

		bundles = malloc(sizeof(void*) * n);
		for(block = &tsfile->head; block; block = block->next)
			bundles[i++] = block->bundle;

		http_server_set_content_type(session, "video/mp2t");
		http_server_send_vec(session, 200, bundles, n);
		hls_file_close(tsfile);
		free(bundles);
	}

	return 0;
}

int hls_server_onhttp(void* param, void* session, const char* method, const char* path)
{
	int r, n;
	char id[256];
	char uri[256];
	char m3u8[MAX_FILES * 48 + 64];
	struct hls_server_t *ctx;
	ctx = (struct hls_server_t *)param;

	// decode request uri
	url_decode(path, -1, uri, sizeof(uri));

	n = strlen(uri);
	if(strstartswith(uri, "/live/"))
	{
		if(n > 11 && strendswith(uri, ".m3u8"))
		{
			return hls_server_live_m3u8(ctx, session, uri);
		}
		else if(strendswith(uri, ".ts"))
		{
			return hls_server_live(ctx, session, uri);
		}
	}

	return hls_server_reply(session, 404, "");
}

int hls_server_init(const char* ip, int port)
{
	return http_server_init();
}

int hls_server_cleanup(void* hls)
{
	return http_server_cleanup();
}

void* hls_server_create(const char* ip, int port)
{
	struct hls_server_t *ctx;

	ctx = (struct hls_server_t *)malloc(sizeof(ctx[0]));
	if(!ctx) return NULL;
	memset(ctx, 0, sizeof(ctx[0]));

	// HTTP server
	ctx->http = http_server_create(ip, port);
	if(!ctx->http)
	{
		free(ctx);
		return NULL;
	}

	// HTTP request handler
	http_server_set_handler(ctx->http, hls_server_onhttp, ctx);

	return ctx;
}

int hls_server_destroy(void* hls)
{
	struct hls_server_t *ctx;
	ctx = (struct hls_server_t *)hls;

	if(ctx->http)
		http_server_destroy(ctx->http);

	free(ctx);
	return 0;
}

int hsl_server_set_handle(void* hls, hls_live_open open, hls_live_close close, void* param)
{
	struct hls_server_t *ctx;
	ctx = (struct hls_server_t *)hls;
	ctx->open = open;
	ctx->close = close;
	ctx->param = param;
	return 0;
}

int hsl_server_input(void* camera, const void* data, int bytes, int stream)
{
	struct hls_live_t *live;
	live = (struct hls_live_t *)camera;
	return hls_live_input(live, data, bytes, stream);
}
