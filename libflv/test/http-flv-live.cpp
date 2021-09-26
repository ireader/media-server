#include "mpeg4-aac.h"
#include "mpeg4-avc.h"
#include "mpeg4-hevc.h"
#include "flv-muxer.h"
#include "flv-writer.h"
#include "http-server.h"
#include "aio-worker.h"
#include "uri-parse.h"
#include "urlcodec.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/path.h"
#include "cstringext.h"
#include <stdio.h>
#include "utf8codec.h"
#include "../deprecated/StdCFile.h"
#include "cpm/shared_ptr.h"
#include <vector>

#define CWD "."

struct http_flv_live_t
{
	char path[PATH_MAX];
	std::shared_ptr<uint8_t> buf;
	size_t cap;
	size_t len;

	http_session_t* http;
	std::shared_ptr<void> writer;
	std::shared_ptr<flv_muxer_t> muxer;
	uint32_t pts, dts;

	std::shared_ptr<void> content;
	std::vector<std::pair<const uint8_t*, int> > frames; // h264 frames
	std::vector<std::pair<const uint8_t*, int> >::const_iterator it;
	const uint8_t* ptr;
	int vcl;
};

static int http_flv_live_read(http_flv_live_t* live);
static int http_flv_live_send(http_flv_live_t* live);

static int http_flv_live_destroy(http_flv_live_t* live)
{
	// TODO: close http sessoin
	delete live;
	return 0;
}

static int http_flv_live_onsend(void* param, int code, size_t bytes)
{
	http_flv_live_t* live = (http_flv_live_t*)param;
	if (0 != code)
	{
		printf("===============send error: %d===============\r\n", code);
		http_flv_live_destroy(live);
		return 0;
	}

	assert(live->len = bytes);
	live->len = 0; // clear

	system_sleep(40); // next frame

	return http_flv_live_send(live);
}

static int http_flv_live_send(http_flv_live_t* live)
{
	if (live->it == live->frames.end())
	{
		printf("===============done===============\r\n");
		http_flv_live_destroy(live);
		return 0; /*end*/
	}

	int r = flv_muxer_avc(live->muxer.get(), live->it->first, live->it->second, live->pts, live->dts);
	live->pts += 40;
	live->dts += 40;
	live->it++;

	if (0 != r)
	{
		printf("===============send error===============\r\n");
		http_flv_live_destroy(live);
		return r;
	}

	r = http_server_send(live->http, live->buf.get(), live->len, http_flv_live_onsend, live);
	return 0 == r ? 1 /*more data*/ : r;
}

static int http_flv_write(void* param, const struct flv_vec_t* v, int n)
{
	http_flv_live_t* live = (http_flv_live_t*)param;

	assert(n <= 3);
	for (int i = 0; i < n; i++)
	{
		if (live->len + v[i].len > live->cap)
		{
			// TODO: realloc
			assert(0);
			return -1;
		}

		// TODO: avoid memory copy
		memcpy(live->buf.get() + live->len, v[i].ptr, v[i].len);
		live->len += v[i].len;
	}
	return 0;
}

static int http_flv_muxer_handler(void* param, int type, const void* data, size_t bytes, uint32_t timestamp)
{
	http_flv_live_t* live = (http_flv_live_t*)param;
	return flv_writer_input(live->writer.get(), type, data, bytes, timestamp);
}

static int http_flv_live(void* http, http_session_t* session, const char* method, const char* path)
{
	char buffer[1024];//[PATH_MAX];
	http_flv_live_t* live = new http_flv_live_t();

	struct uri_t* uri = uri_parse(path, (int)strlen(path));
	url_decode(uri->path, -1, live->path, sizeof(live->path));
	uri_free(uri);

	int n = 0;
	while (live->path[n] == '/') n++; // filter /
	UTF8Decode utf8(live->path+n);

	path_resolve(buffer, sizeof(buffer), utf8, CWD, strlen(CWD));
	// TODO: path resolve to fix /rootpath/../pathtosystem -> /pathtosystem
	path_realpath(buffer, live->path);

	// read frames
	int r = http_flv_live_read(live);
	if (0 != r)
	{
		delete live;
		http_server_set_status_code(session, r, NULL);
		return http_server_send(session, "", 0, NULL, NULL);
	}

	live->http = session;
	http_server_set_header(session, "Connection", "close"); // server close connection on finish
	http_server_set_content_length(session, -1); // don't need content-length

	live->pts = 0;
	live->dts = 0;
	live->it = live->frames.begin();
	live->muxer.reset(flv_muxer_create(http_flv_muxer_handler, live), flv_muxer_destroy);
	
	// write flv file header
	live->writer.reset(flv_writer_create2(0, 1, http_flv_write, live), flv_writer_destroy);
	return http_flv_live_send(live);
}

void http_flv_live_test(const char* ip, int port)
{
	aio_worker_init(4);
	http_server_t* http = http_server_create(ip, port);
	http_server_set_handler(http, http_flv_live, http);

	// http process
	while ('q' != getchar())
	{
	}

	http_server_destroy(http);
	aio_worker_clean(4);
}

static void h264_handler(void* param, const uint8_t* nalu, size_t bytes)
{
	http_flv_live_t* ctx = (http_flv_live_t*)param;
	assert(ctx->ptr < nalu);

	const uint8_t* ptr = nalu - 3;
	//const uint8_t* end = (const uint8_t*)nalu + bytes;
	uint8_t nalutype = nalu[0] & 0x1f;
	if (ctx->vcl > 0 && h264_is_new_access_unit(nalu, bytes))
	{
		ctx->cap = ptr - ctx->ptr > ctx->cap ? ptr - ctx->ptr : ctx->cap; // max
		ctx->frames.push_back(std::make_pair(ctx->ptr, ptr - ctx->ptr));
		ctx->ptr = ptr;
		ctx->vcl = 0;
	}

	if (1 <= nalutype && nalutype <= 5)
		++ctx->vcl;
}

static int http_flv_live_read(http_flv_live_t* live)
{
	StdCFile fp(live->path, "rb");
	if (!fp.IsOpened())
		return 404;

	long n = fp.GetFileSize();
	if (n >= 32 * 1024 * 1024)
		return 401;

	live->content.reset(fp.Read(), free);
	if (!live->content.get())
		return 500;

	live->vcl = 0;
	live->ptr = (const uint8_t*)live->content.get();
	live->cap = 0;
	live->len = 0;

	if (strendswith(live->path, ".h264") || strendswith(live->path, ".264"))
	{
		mpeg4_h264_annexb_nalu(live->content.get(), n, h264_handler, live);

		live->cap += 4 * 1024; // flv avc/aac sequence header
		live->buf.reset((uint8_t*)malloc(live->cap), free);
	}
	else
	{
		return 500;
	}

	return 0;
}
