#include <stdlib.h>
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

static int hls_server_m3u8_parse(const char* path, char* name, size_t szname, char* server, char* key)
{
    size_t n;
 //   void *content;

    n = strlen(path);
    if(n - 6 - 5 >= szname) // 6-/live/, 5-.m3u8
        return -1;
    
    strncpy(name, path + 6, n - 6 - 5);
    name[n - 6 - 5] = '\0';

    //	n = 0;
    //	http_server_get_content(session, &content, &n);
	server[0] = '\0';
	key[0] = '\0';
    return 0;
}

// /live/name.m3u8
static int hls_server_live_m3u8(struct hls_server_t *ctx, void* session, const char* path)
{
    int r;
	char name[64];
	char server[64];
	char key[64];
	//char m3u8[512];
	char *m3u8;
	time64_t ltnow;
	void* bundle;
	struct hls_live_t *live;

    // parse server/key
    r = hls_server_m3u8_parse(path, name, sizeof(name), server, key);
    if(0 != r)
    {
        hls_server_reply(session, 404, "");
        return r;
    }

    // fetch live object
	live = hls_live_fetch(name);
	if(!live)
		return hls_server_reply(session, 500, "");

	// create a new thread?
	if(0 == live->opened)
	{
        r = ctx->open ? ctx->open(ctx->param, live, name, server, key) : -1;
		if(0 != r)
        {
            hls_server_reply(session, 404, "");
            hls_live_release(live);
            return r;
        }

        live->opened = 1; // TODO: close???
	}

	// VLC need at least 3-file
	ltnow = time64_now();
	while(live->file_count < 2 && time64_now() - ltnow < HLS_DURATION *1000)
	{
		system_sleep(200);
	}

	bundle = http_bundle_alloc(128 * HLS_FILE_NUM);
	m3u8 = (char*)http_bundle_lock(bundle);
    hls_live_m3u8(live, m3u8);
    printf("\n%s\n", m3u8);

	http_bundle_unlock(bundle, strlen(m3u8));
	http_server_set_content_type(session, HLS_M3U8_TYPE);
	http_server_send(session, 200, bundle);
    http_bundle_free(bundle);

    hls_live_release(live); // release fetch reference count
	return 0;
}

static int hls_server_live_parse(const char* path, char* name, size_t szname, char* file, size_t szfile)
{
    // /live/name/1.ts
    size_t n;
    const char* p;

    n = strlen(path);
    assert(n < szname + szfile + 6);

    p = strrchr(path, '/');
    if(n - (p - path) > szfile || (size_t)(p - path) > szname || p - path < 7)
        return -1;

    strncpy(name, path + 6, p - path - 6);
    strncpy(file, p + 1, n - (p + 1 - path) - 3);
    name[p - path - 6] = '\0';
    file[n - (p + 1 - path) - 3] = '\0';
    return 0;
}

static int hls_server_live(struct hls_server_t *ctx, void* session, const char* path)
{
#if 0
    int r;
    void* bundle;
    void* ptr;
    FILE *fp;
    
    bundle = http_bundle_alloc(4*1024*1024);
    ptr = http_bundle_lock(bundle);
    
    fp = fopen("/Users/a360/Downloads/1.ts", "rb");
    r = fread(ptr, 1, 4*1024*1024, fp);
    fclose(fp);
    
    http_bundle_unlock(bundle, r);
    
    http_server_send(session, 200, bundle);
    http_bundle_free(bundle);
#else
	int i, r, n;
	char name[64];
	char file[64];
	void** bundles;
    struct list_head *pos;
	struct hls_live_t *live;
	struct hls_file_t *tsfile;
	struct hls_block_t *block;

	r = hls_server_live_parse(path, name, sizeof(name), file, sizeof(file));
    if(0 != r)
        return hls_server_reply(session, 404, "");

    live = hls_live_fetch(name);
    if(!live)
        return hls_server_reply(session, 500, "");

    tsfile = hls_live_file(live, file);
    if(!tsfile)
    {
        hls_live_release(live);
        return hls_server_reply(session, 404, "");
    }

    i = 0;
    n = 0;
    list_for_each(pos, &tsfile->head)
    {
        ++n;
    }

    bundles = (void**)malloc(sizeof(void*) * n);
    if(!bundles)
    {
        hls_file_close(tsfile);
        hls_live_release(live);
        return hls_server_reply(session, 404, "");
    }

    list_for_each(pos, &tsfile->head)
    {
        block = list_entry(pos, struct hls_block_t, link);
        bundles[i++] = block->bundle;
    }

    printf("live read: %s/%s.ts\n", name, file);
    http_server_set_content_type(session, HLS_TS_TYPE);
    http_server_send_vec(session, 200, bundles, n);

    hls_file_close(tsfile);
    hls_live_release(live); // release fetched live object
    free(bundles);
#endif

	return 0;
}

static int hls_server_onhttp(void* param, void* session, const char* method, const char* path)
{
	size_t n;
	char uri[256];
	const char* p;
	struct hls_server_t *ctx;
	ctx = (struct hls_server_t *)param;

	// decode request uri
	url_decode(path, -1, uri, sizeof(uri));

	n = strlen(uri);
	if(0 == strncmp(uri, "/live/", 6))
	{
		p = strrchr(uri, '.');
		if (p && 0 == strcmp(p, ".m3u8"))
		{
			return hls_server_live_m3u8(ctx, session, uri);
		}
		else if (p && 0 == strcmp(p, ".ts"))
		{
			return hls_server_live(ctx, session, uri);
		}
	}

	return hls_server_reply(session, 404, "");
}

int hls_server_init()
{
    hls_live_init();
	return http_server_init();
}

int hls_server_cleanup()
{
    hls_live_cleanup();
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
    int r = 0;
	struct hls_live_t *live;
    static unsigned char s_audio[] = {0xff, 0xf1, 0x5c, 0x40, 0x01, 0x7f, 0xfc, 0x00, 0xd0, 0x40, 0x07};

#if 1
    int i;
    int j = 0;
    const unsigned char* p = (const unsigned char*)data;

    live = (struct hls_live_t *)camera;
    for(i = 0; i + 4 < bytes; i++)
    {
        if(0x00 == p[i] && 0x00 == p[i+1] && 0x01 == p[i+2])
        {
            int naltype = p[i+3] & 0x1f;
            if(7 != naltype && 8 != naltype && 9 != naltype)
            {
                while(j > 0 && 0x00==live->vbuffer[j-1])
                {
                    --j; // remove zero_bytes;
                }
            }
        }

        live->vbuffer[j++] = p[i];
    }

    while(i < bytes)
        live->vbuffer[j++] = p[i++];
    data = live->vbuffer;
    bytes = j;
#endif

    live = (struct hls_live_t *)camera;
    if(HLS_VIDEO_H264 == stream)
    {
        r = hls_live_input(live, data, bytes, stream);
        r = hls_live_input(live, s_audio, sizeof(s_audio), 0x0f);
    }
    return r;
}
