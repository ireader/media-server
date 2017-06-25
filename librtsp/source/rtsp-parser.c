#include "rtsp-parser.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64) || defined(OS_WINDOWS)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#define KB (1024)
#define MB (1024*1024)

enum { SM_START_LINE = 0, SM_HEADER = 100, SM_BODY = 200, SM_DONE = 300 };

struct rtsp_string_t
{
	size_t pos; // offset from raw data
	size_t len;
};

struct rtsp_status_line_t
{
	int code;
	struct rtsp_string_t reason;
};

struct rtsp_request_line_t
{
	struct rtsp_string_t method;
	struct rtsp_string_t uri;
};

struct rtsp_header_t
{
	struct rtsp_string_t name;
	struct rtsp_string_t value;
};

struct rtsp_chunk_t
{
	size_t offset;
	size_t len;
	size_t pos;
};

struct rtsp_context
{
	char *raw;
	size_t raw_size;
	size_t raw_capacity;
	int server_mode; // 0-client, 1-server
	int stateM;
	size_t offset;

	struct rtsp_header_t header;
	struct rtsp_chunk_t chunk;

	// start line
	int verminor, vermajor;
	union
	{
		struct rtsp_request_line_t req;
		struct rtsp_status_line_t reply;
	} u;

	// headers
	struct rtsp_header_t *headers;
	int header_size; // the number of http header
	int header_capacity;
	int content_length; // -1-don't have header, >=0-Content-Length
	int connection_close; // 1-close, 0-keep-alive, <0-don't set
	int content_encoding;
	int transfer_encoding;
	int cookie;
	int location;
};

static unsigned int s_body_max_size = 0*MB;


// RFC 2612 H2.2
// token = 1*<any CHAR except CTLs or separators>
// separators = "(" | ")" | "<" | ">" | "@"
//				| "," | ";" | ":" | "\" | <">
//				| "/" | "[" | "]" | "?" | "="
//				| "{" | "}" | SP | HT
static inline int is_valid_token(const char* s, int len)
{
	const char *p;
	for(p = s; p < s + len && *p; ++p)
	{
		// CTLs or separators
		if(*p <= 31 || *p >= 127 || !!strchr("()<>@,;:\\\"/[]?={} \t", *p))
			break;
	}

	return p == s+len ? 1 : 0;
}

static inline int is_server_mode(struct rtsp_context *ctx)
{
	return RTSP_PARSER_SERVER==ctx->server_mode ? 1 : 0;
}

static inline int is_transfer_encoding_chunked(struct rtsp_context *ctx)
{
	return (ctx->transfer_encoding>0 && 0==strncasecmp("chunked", ctx->raw+ctx->transfer_encoding, 7)) ? 1 : 0;
}

static int rtsp_rawdata(struct rtsp_context *ctx, const void* data, int bytes)
{
	void *p;
	int capacity;

	if(ctx->raw_capacity - ctx->raw_size < (size_t)bytes + 1)
	{
		capacity = (ctx->raw_capacity > 4*MB) ? 50*MB : (ctx->raw_capacity > 16*KB ? 2*MB : 8*KB);
		capacity = (bytes + 1) > capacity ? (bytes + 1) : capacity;
		p = realloc(ctx->raw, ctx->raw_capacity + capacity);
		if(!p)
			return ENOMEM;

		ctx->raw_capacity += capacity;
		ctx->raw = p;
	}

	assert(ctx->raw_capacity - ctx->raw_size > (size_t)bytes+1);
	memmove((char*)ctx->raw + ctx->raw_size, data, bytes);
	ctx->raw_size += bytes;
	ctx->raw[ctx->raw_size] = '\0'; // auto add ending '\0'
	return 0;
}

// general-header = Cache-Control ; Section 14.9
//					| Connection ; Section 14.10
//					| Date ; Section 14.18
//					| Pragma ; Section 14.32
//					| Trailer ; Section 14.40
//					| Transfer-Encoding ; Section 14.41
//					| Upgrade ; Section 14.42
//					| Via ; Section 14.45
//					| Warning ; Section 14.46
//
// request-header = Accept ; Section 14.1
//					| Accept-Charset ; Section 14.2
//					| Accept-Encoding ; Section 14.3
//					| Accept-Language ; Section 14.4
//					| Authorization ; Section 14.8
//					| Expect ; Section 14.20
//					| From ; Section 14.22
//					| Host ; Section 14.23
//					| If-Match ; Section 14.24
//					| If-Modified-Since ; Section 14.25
//					| If-None-Match ; Section 14.26
//					| If-Range ; Section 14.27
//					| If-Unmodified-Since ; Section 14.28
//					| Max-Forwards ; Section 14.31
//					| Proxy-Authorization ; Section 14.34
//					| Range ; Section 14.35
//					| Referer ; Section 14.36
//					| TE ; Section 14.39
//					| User-Agent ; Section 14.43
//
// response-header = Accept-Ranges ; Section 14.5
//					| Age ; Section 14.6
//					| ETag ; Section 14.19
//					| Location ; Section 14.30
//					| Proxy-Authenticate ; Section 14.33
//					| Retry-After ; Section 14.37
//					| Server ; Section 14.38
//					| Vary ; Section 14.44
//					| WWW-Authenticate ; Section 14.47
//
// entity-header = Allow ; Section 14.7
//					| Content-Encoding ; Section 14.11
//					| Content-Language ; Section 14.12
//					| Content-Length ; Section 14.13
//					| Content-Location ; Section 14.14
//					| Content-MD5 ; Section 14.15
//					| Content-Range ; Section 14.16
//					| Content-Type ; Section 14.17
//					| Expires ; Section 14.21
//					| Last-Modified ; Section 14.29
//					| extension-header
//
// extension-header = message-header
static int rtsp_header_handler(struct rtsp_context *ctx, size_t npos, size_t vpos)
{
	const char* name = ctx->raw + npos;
	const char* value = ctx->raw + vpos;

	if(0 == strcasecmp("Content-Length", name))
	{
		// H4.4 Message Length, section 3, ignore content-length if in chunked mode
		if(is_transfer_encoding_chunked(ctx))
			ctx->content_length = -1;
		else
			ctx->content_length = atoi(value);
		assert(ctx->content_length >= 0 && (0==s_body_max_size || ctx->content_length < (int)s_body_max_size));
	}
	else if(0 == strcasecmp("Connection", name))
	{
		ctx->connection_close = (0 == strcasecmp("close", value)) ? 1 : 0;
	}
	else if(0 == strcasecmp("Content-Encoding", name))
	{
		// gzip/compress/deflate/identity(default)
		ctx->content_encoding = (int)vpos;
	}
	else if(0 == strcasecmp("Transfer-Encoding", name))
	{
		ctx->transfer_encoding = (int)vpos;
		if(0 == strncasecmp("chunked", value, 7))
		{
			// chunked can't use with content-length
			// H4.4 Message Length, section 3,
			assert(-1 == ctx->content_length);
			ctx->raw[ctx->transfer_encoding + 7] = '\0'; // ignore parameters
		}
	}
	else if(0 == strcasecmp("Set-Cookie", name))
	{
		ctx->cookie = (int)vpos;
	}
	else if(0 == strcasecmp("Location", name))
	{
		ctx->location = (int)vpos;
	}

	return 0;
}

static int rtsp_header_add(struct rtsp_context *ctx, struct rtsp_header_t* header)
{
	int size;
	struct rtsp_header_t *p;
	if(ctx->header_size+1 >= ctx->header_capacity)
	{
		size = ctx->header_capacity < 16 ? 16 : (ctx->header_size * 3 / 2);
		p = (struct rtsp_header_t*)realloc(ctx->headers, sizeof(struct rtsp_header_t) * size);
		if(!p)
			return ENOMEM;

		ctx->headers = p;
		ctx->header_capacity = size;
	}

	assert(header->name.pos > 0);
	assert(header->name.len > 0);
	assert(header->value.pos > 0);
	assert(is_valid_token(ctx->raw + header->name.pos, header->name.len));
	ctx->raw[header->name.pos + header->name.len] = '\0';
	ctx->raw[header->value.pos + header->value.len] = '\0';
	memcpy(ctx->headers + ctx->header_size, header, sizeof(struct rtsp_header_t));
	++ctx->header_size;
	return 0;
}

// H5.1 Request-Line
// Request-Line = Method SP Request-URI SP RTSP-Version CRLF
// GET http://www.w3.org/pub/WWW/TheProject.html HTTP/1.1
static int rtsp_parse_request_line(struct rtsp_context *ctx)
{
	enum {
		SM_REQUEST_START = SM_START_LINE,
		SM_REQUEST_METHOD,
		SM_REQUEST_SP1,
		SM_REQUEST_URI,
		SM_REQUEST_SP2,
		SM_REQUEST_VERSION,
		SM_REQUEST_SP3,
		SM_REQUEST_CR,
		SM_REQUEST_LF,
	};

	char c;
	size_t *v[6];
	v[0] = &ctx->u.req.method.pos;
	v[1] = &ctx->u.req.method.len;
	v[2] = &ctx->u.req.uri.pos;
	v[3] = &ctx->u.req.uri.len;
	v[4] = &ctx->header.name.pos; // version
	v[5] = &ctx->header.name.len;

	for (; ctx->offset < ctx->raw_size; ++ctx->offset)
	{
		c = ctx->raw[ctx->offset];
		switch (ctx->stateM)
		{
		case SM_REQUEST_START:
		case SM_REQUEST_SP1:
		case SM_REQUEST_SP2:
			if (' ' != c && '\t' != c)
			{
				*(v[ctx->stateM - SM_REQUEST_START]) = ctx->offset;
				ctx->stateM += 1; // next state
				ctx->offset -= 1; // go back
			}
			break;

		case SM_REQUEST_METHOD:
		case SM_REQUEST_URI:
		case SM_REQUEST_VERSION:
			if (' ' == c || '\t' == c || '\r' == c || '\n' == c)
			{
				*(v[ctx->stateM - SM_REQUEST_START]) = ctx->offset - *(v[ctx->stateM - SM_REQUEST_START - 1]);
				ctx->stateM += 1; // next state
				ctx->offset -= 1; // go back
			}
			break;

		case SM_REQUEST_SP3:
			switch (c)
			{
			case ' ':
			case '\t':
				break; // continue

			case '\r':
				ctx->stateM = SM_REQUEST_CR;
				break;

			case '\n':
				ctx->stateM = SM_REQUEST_LF;
				break;

			default:
				ctx->stateM = SM_REQUEST_VERSION;
				break;
			}
			break;

		case SM_REQUEST_CR:
			if ('\n' != c)
			{
				assert(0);
				return -1;
			}
			ctx->stateM = SM_REQUEST_LF;
			break;

		case SM_REQUEST_LF:
			// H5.1.1 Method (p24)
			// Method = OPTIONS | GET | HEAD | POST | PUT | DELETE | TRACE | CONNECT | extension-method
			if (ctx->u.req.method.len < 1
				// H5.1.2 Request-URI (p24)
				// Request-URI = "*" | absoluteURI | abs_path | authority
				|| ctx->u.req.uri.len < 1
				// H3.1 HTTP Version (p13)
				// HTTP-Version = "HTTP" "/" 1*DIGIT "." 1*DIGIT
				|| ctx->header.name.len < 8
				|| 2 != sscanf(ctx->raw + ctx->header.name.pos, "RTSP/%d.%d", &ctx->vermajor, &ctx->verminor))
			{
				assert(0);
				return -1;
			}

			ctx->raw[ctx->u.req.method.pos + ctx->u.req.method.len] = '\0';
			ctx->raw[ctx->u.req.uri.pos + ctx->u.req.uri.len] = '\0';
			assert(1 == ctx->vermajor || 2 == ctx->vermajor);
			assert(1 == ctx->verminor || 0 == ctx->verminor);
			ctx->stateM = SM_HEADER;
			return 0;

		default:
			assert(0);
			return -1; // invalid
		}
	}

	return 0; // wait for more data
}

// H6.1 Status-Line
// Status-Line = RTSP-Version SP Status-Code SP Reason-Phrase CRLF
static int rtsp_parse_status_line(struct rtsp_context *ctx)
{
	enum {
		SM_STATUS_START = SM_START_LINE,
		SM_STATUS_VERSION,
		SM_STATUS_SP1,
		SM_STATUS_CODE,
		SM_STATUS_SP2,
		SM_STATUS_REASON,
		SM_STATUS_SP3,
		SM_STATUS_CR,
		SM_STATUS_LF,
	};

	char c;
	size_t *v[6];
	v[0] = &ctx->header.name.pos; // version
	v[1] = &ctx->header.name.len;
	v[2] = &ctx->header.value.pos; // status code
	v[3] = &ctx->header.value.len;
	v[4] = &ctx->u.reply.reason.pos;
	v[5] = &ctx->u.reply.reason.len;

	for (; ctx->offset < ctx->raw_size; ctx->offset++)
	{
		c = ctx->raw[ctx->offset];
		switch (ctx->stateM)
		{
		case SM_STATUS_START:
		case SM_STATUS_SP1:
		case SM_STATUS_SP2:
			if (' ' != c && '\t' != c)
			{
				*(v[ctx->stateM - SM_STATUS_START]) = ctx->offset;
				ctx->stateM += 1; // next state
				ctx->offset -= 1; // go back
			}
			break;

		case SM_STATUS_VERSION:
		case SM_STATUS_CODE:
		case SM_STATUS_REASON:
			if (' ' == c || '\t' == c || '\r' == c || '\n' == c)
			{
				*(v[ctx->stateM - SM_STATUS_START]) = ctx->offset - *(v[ctx->stateM - SM_STATUS_START - 1]);
				ctx->stateM += 1; // next state
				ctx->offset -= 1; // go back
			}
			break;

		case SM_STATUS_SP3:
			switch (c)
			{
			case ' ':
			case '\t':
				break; // continue

			case '\r':
				ctx->stateM = SM_STATUS_CR;
				break;

			case '\n':
				ctx->stateM = SM_STATUS_LF;
				break;

			default:
				ctx->stateM = SM_STATUS_REASON;
				break;
			}
			break;

		case SM_STATUS_CR:
			if ('\n' != c)
			{
				assert(0);
				return -1;
			}
			ctx->stateM = SM_STATUS_LF;
			break;

		case SM_STATUS_LF:
			// H3.1 HTTP Version (p13)
			// HTTP-Version = "HTTP" "/" 1*DIGIT "." 1*DIGIT
			if (ctx->header.name.len < 8 || 2 != sscanf(ctx->raw + ctx->header.name.pos, "RTSP/%d.%d", &ctx->vermajor, &ctx->verminor)
				// H6.1.1 Status Code and Reason Phrase (p26)
				// The Status-Code element is a 3-digit integer result code
				|| ctx->header.value.len != 3 || ctx->u.reply.reason.len < 1)
			{
				assert(0);
				return -1;
			}

			ctx->raw[ctx->u.reply.reason.pos + ctx->u.reply.reason.len] = '\0';
			ctx->raw[ctx->header.value.pos + ctx->header.value.len] = '\0';
			ctx->u.reply.code = atoi(ctx->raw + ctx->header.value.pos);
			assert(1 == ctx->vermajor || 2 == ctx->vermajor);
			assert(1 == ctx->verminor || 0 == ctx->verminor);
			ctx->stateM = SM_HEADER;
			return 0;

		default:
			assert(0);
			return -1; // invalid
		}
	}

	return 0;
}

// H4.2 Message Headers
// message-header = field-name ":" [ field-value ]
// field-name = token
// field-value = *( field-content | LWS )
// field-content = <the OCTETs making up the field-value
//					and consisting of either *TEXT or combinations
//					of token, separators, and quoted-string>
static int rtsp_parse_header_line(struct rtsp_context *ctx)
{
	enum {
		SM_HEADER_START = SM_HEADER,
		SM_HEADER_NAME,
		SM_HEADER_SP1,
		SM_HEADER_SEPARATOR,
		SM_HEADER_SP2,
		SM_HEADER_VALUE,
		SM_HEADER_SP3,
		SM_HEADER_CR,
		SM_HEADER_LF,
	};

	char c;
	size_t dummy;
	size_t *v[6];

	v[0] = &ctx->header.name.pos;
	v[1] = &ctx->header.name.len;
	v[2] = &dummy;
	v[3] = &dummy;
	v[4] = &ctx->header.value.pos;
	v[5] = &ctx->header.value.len;

	for (; ctx->offset < ctx->raw_size; ctx->offset++)
	{
		c = ctx->raw[ctx->offset];
		switch (ctx->stateM)
		{
		case SM_HEADER_START:
			switch (c)
			{
			case ' ':
			case '\t':
				// H2.2 Basic Rules (p12)
				// HTTP/1.1 header field values can be folded onto multiple lines if the continuation line begins with a space or
				// horizontal tab.All linear white space, including folding, has the same semantics as SP.A recipient MAY replace any
				// linear white space with a single SP before interpreting the field value or forwarding the message downstream.
				ctx->header.name.pos = 0; // use for multiple lines flag
				ctx->stateM = SM_HEADER_SP2; // next state
				break;

			case '\r':
				ctx->header.value.pos = 0; // use for header end flag
				ctx->stateM = SM_HEADER_CR;
				break;

			case '\n':
				ctx->header.value.pos = 0; // use for header end flag
				ctx->stateM = SM_HEADER_LF;
				break;

			default:
				ctx->header.name.pos = ctx->offset;
				ctx->stateM += 1; // next state
				ctx->offset -= 1; // go back
				break;
			}
			break;

		case SM_HEADER_SP1:
		case SM_HEADER_SP2:
			if (' ' != c && '\t' != c)
			{
				*(v[ctx->stateM - SM_HEADER_START]) = ctx->offset;
				ctx->stateM += 1; // next state
				ctx->offset -= 1; // go back
			}
			break;

		case SM_HEADER_NAME:
		case SM_HEADER_VALUE:
			if (' ' == c || '\t' == c || '\r' == c || '\n' == c || (':' == c && SM_HEADER_NAME == ctx->stateM))
			{
				*(v[ctx->stateM - SM_HEADER_START]) = ctx->offset - *(v[ctx->stateM - SM_HEADER_START - 1]);
				ctx->stateM += 1; // next state
				ctx->offset -= 1; // go back
			}
			break;

		case SM_HEADER_SEPARATOR:
			if (':' != c)
			{
				assert(0);
				return -1;
			}

			// accept
			ctx->stateM += 1; // next state
			break;

		case SM_HEADER_SP3:
			switch (c)
			{
			case ' ':
			case '\t':
				break; // continue

			case '\r':
				ctx->stateM = SM_HEADER_CR;
				break;

			case '\n':
				ctx->stateM = SM_HEADER_LF;
				break;

			default:
				ctx->stateM = SM_HEADER_VALUE;
				break;
			}
			break;

		case SM_HEADER_CR:
			if ('\n' != c)
			{
				assert(0);
				return -1;
			}
			ctx->stateM = SM_HEADER_LF;
			break;

		case SM_HEADER_LF:
			if (0 == ctx->header.value.pos)
			{
				ctx->stateM = SM_BODY;
				return 0;
			}

			if (0 == ctx->header.name.pos)
			{
				// multiple lines header
				size_t i;
				struct rtsp_header_t* h;

				if (ctx->header_size < 1)
				{
					assert(0);
					return -1;
				}

				h = &ctx->headers[ctx->header_size - 1];
				for (i = h->value.len; i < ctx->header.value.pos; i++)
				{
					ctx->raw[i] = ' '; // replace with SP
				}
				h->value.len = ctx->header.value.pos - h->value.pos + ctx->header.value.len;
				ctx->raw[h->value.pos + h->value.len] = '\0';

				rtsp_header_handler(ctx, h->name.pos, h->value.pos); // handle
			}
			else
			{
				if (ctx->header.name.len < 1)
				{
					assert(0);
					return -1;
				}

				if (0 != rtsp_header_add(ctx, &ctx->header))
					return -1;

				rtsp_header_handler(ctx, ctx->header.name.pos, ctx->header.value.pos); // handle
			}

			ctx->stateM = SM_HEADER; // continue
			ctx->offset -= 1; // go back
			break;

		default:
			assert(0);
			return -1;
		}
	}

	return 0;
}

// H3.6.1 Chunked Transfer Coding
// Chunked-Body		= *chunk
//					  last-chunk
//					  trailer
//					  CRLF
//	chunk			= chunk-size [ chunk-extension ] CRLF
//					  chunk-data CRLF
//	chunk-size		= 1*HEX
//	last-chunk		= 1*("0") [ chunk-extension ] CRLF
//	chunk-extension	= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
//	chunk-ext-name	= token
//	chunk-ext-val	= token | quoted-string
//	chunk-data		= chunk-size(OCTET)
//	trailer			= *(entity-header CRLF)
static int rtsp_parse_chunked(struct rtsp_context *ctx)
{
	enum {
		CHUNK_START = SM_BODY,
		CHUNK_SIZE,
		CHUNK_EXTENSION,
		CHUNK_EXTENSION_CR,
		CHUNK_DATA,
		CHUNK_TRAILER_START,
		CHUNK_TRAILER,
		CHUNK_TRAILER_CR,
		CHUNK_END,
		CHUNK_END_CR,
	};

	char c;
	assert(is_transfer_encoding_chunked(ctx));
	if(0 == ctx->chunk.offset)
	{
		ctx->chunk.offset = ctx->offset;
		assert(-1 == ctx->content_length);
		ctx->content_length = 0;
	}

	for(; ctx->chunk.offset < ctx->raw_size; ctx->chunk.offset++)
	{
		c = ctx->raw[ctx->chunk.offset];

		switch(ctx->stateM)
		{
		case CHUNK_START:
			assert(0 == ctx->chunk.len);
			if('0' <= c && c <= '9')
			{
				ctx->chunk.len = c - '0';
			}
			else if('a' <= c && c <= 'f')
			{
				ctx->chunk.len = c - 'a' + 10;
			}
			else if('A' <= c && c <= 'F')
			{
				ctx->chunk.len = c - 'A' + 10;
			}
			else
			{
				assert(0);
				return -1;
			}

			ctx->stateM = CHUNK_SIZE;
			break;

		case CHUNK_SIZE:
			if('0' <= c && c <= '9')
			{
				ctx->chunk.len = ctx->chunk.len * 16 + (c - '0');
			}
			else if('a' <= c && c <= 'f')
			{
				ctx->chunk.len = ctx->chunk.len * 16 + (c - 'a' + 10);
			}
			else if('A' <= c && c <= 'F')
			{
				ctx->chunk.len = ctx->chunk.len * 16 + (c - 'A' + 10);
			}
			else
			{
				switch(c)
				{
				case '\t':
				case ' ':
				case ';':
					ctx->stateM = CHUNK_EXTENSION;
					break;

				case '\r':
					ctx->stateM = CHUNK_EXTENSION_CR;
					break;

				case '\n':
					ctx->chunk.pos = ctx->chunk.offset + 1;
					ctx->stateM = 0==ctx->chunk.len ? CHUNK_TRAILER_START : CHUNK_DATA;
					break;

				default:
					assert(0);
					return -1;
				}
			}
			break;

		case CHUNK_EXTENSION:
			switch(c)
			{
			case '\r':
				ctx->stateM = CHUNK_EXTENSION_CR;
				break;

			case '\n':
				ctx->chunk.pos = ctx->chunk.offset + 1;
				ctx->stateM = 0==ctx->chunk.len ? CHUNK_TRAILER_START : CHUNK_DATA;
				break;
			}
			break;

		case CHUNK_EXTENSION_CR:
			if('\n' != c)
			{
				assert(0);
				return -1;
			}

			ctx->chunk.pos = ctx->chunk.offset + 1;
			ctx->stateM = 0==ctx->chunk.len ? CHUNK_TRAILER_START : CHUNK_DATA;
			break;

		case CHUNK_DATA:
			assert(ctx->chunk.len > 0);
			assert(0 != ctx->chunk.pos);
			if(ctx->chunk.pos + ctx->chunk.len + 2 > ctx->raw_size)
				return 0; // wait for more data

			if('\r' != ctx->raw[ctx->chunk.pos + ctx->chunk.len] || '\n' != ctx->raw[ctx->chunk.pos + ctx->chunk.len + 1])
			{
				assert(0);
				return -1;
			}

			memmove(ctx->raw+ctx->offset+ctx->content_length, ctx->raw+ctx->chunk.pos, ctx->chunk.len);
			ctx->raw[ctx->offset+ctx->content_length+ctx->chunk.len] = '\0';
			ctx->content_length += ctx->chunk.len;
			ctx->stateM = CHUNK_START;

			ctx->chunk.offset += ctx->chunk.len + 1; // skip \r\n
			ctx->chunk.pos = ctx->chunk.len = 0; // reuse chunk
			break;

		case CHUNK_TRAILER_START:
			switch(c)
			{
			case '\r':
				ctx->stateM = CHUNK_END_CR;
				break;

			case '\n':
				++ctx->chunk.offset;
				ctx->stateM = SM_DONE;
				return 0;

			default:
				ctx->stateM = CHUNK_TRAILER;
				break;
			}
			break;

		case CHUNK_TRAILER:
			switch(c)
			{
			case '\r':
				ctx->stateM = CHUNK_TRAILER_CR;
				break;

			case '\n':
				ctx->stateM = CHUNK_TRAILER_START;
				break;
			}
			break;

		case CHUNK_TRAILER_CR:
			if('\n' != c)
			{
				assert(0);
				return -1;
			}
			ctx->stateM = CHUNK_TRAILER_START;
			break;

		case CHUNK_END:
			switch(c)
			{
			case '\r':
				ctx->stateM = CHUNK_END_CR;
				break;

			case '\n':
				++ctx->chunk.offset;
				ctx->stateM = SM_DONE;
				return 0;

			default:
				assert(0);
				return -1;
			}
			break;

		case CHUNK_END_CR:
			if('\n' != c)
			{
				assert(0);
				return -1;
			}
			ctx->stateM = SM_DONE;
			++ctx->chunk.offset;
			return 0;
		}
	}

	return 0;
}

void* rtsp_parser_create(enum RTSP_PARSER_MODE mode)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)malloc(sizeof(struct rtsp_context));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(struct rtsp_context));
	ctx->server_mode = mode;
	rtsp_parser_clear(ctx);
	return ctx;
}

int rtsp_parser_destroy(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	if(ctx->raw)
	{
		assert(ctx->raw_capacity > 0);
		free(ctx->raw);
		ctx->raw = 0;
		ctx->raw_size = 0;
		ctx->raw_capacity = 0;
	}

	if(ctx->headers)
	{
		assert(ctx->header_capacity > 0);
		free(ctx->headers);
		ctx->headers = 0;
		ctx->header_size = 0;
		ctx->header_capacity = 0;
	}

	free(ctx);
	return 0;
}

void rtsp_parser_clear(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;

	memset(&ctx->u.req, 0, sizeof(ctx->u.req));
	memset(&ctx->u.reply, 0, sizeof(ctx->u.reply));
	memset(&ctx->chunk, 0, sizeof(struct rtsp_chunk_t));
	ctx->stateM = SM_START_LINE;
	ctx->offset = 0;
	ctx->raw_size = 0;
	ctx->header_size = 0;
	ctx->content_length = -1;
	ctx->connection_close = -1;
	ctx->content_encoding = 0;
	ctx->transfer_encoding = 0;
	ctx->cookie = 0;
	ctx->location = 0;
}

int rtsp_parser_input(void* parser, const void* data, int *bytes)
{
	enum { INPUT_NEEDMORE = 1, INPUT_DONE = 0, };

	int r;
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;

	// save raw data
	r = rtsp_rawdata(ctx, data, *bytes);
	if(0 != r)
	{
		assert(r < 0);
		return r;
	}

	if(SM_START_LINE <= ctx->stateM && ctx->stateM < SM_HEADER)
	{
		r = is_server_mode(ctx) ? rtsp_parse_request_line(ctx) : rtsp_parse_status_line(ctx);
	}

	if(SM_HEADER <= ctx->stateM && ctx->stateM < SM_BODY)
	{
		r = rtsp_parse_header_line(ctx);
	}

	assert(r <= 0);
	if(SM_BODY <= ctx->stateM && ctx->stateM < SM_DONE)
	{
		if(is_transfer_encoding_chunked(ctx))
		{
			r = rtsp_parse_chunked(ctx);
		}
		else
		{
			if(-1 == ctx->content_length)
			{
				// RTSP2326 4.4 4.4 Message Length
				ctx->content_length = 0;
				ctx->stateM = SM_DONE;
			}
			else
			{
				assert(ctx->raw_size <= ctx->offset + ctx->content_length);
				if(ctx->raw_size >= ctx->offset + ctx->content_length)
					ctx->stateM = SM_DONE;
			}
		}
	}

	if(r < 0)
		return r;

	*bytes = 0;
	return ctx->stateM == SM_DONE ? INPUT_DONE : INPUT_NEEDMORE;
}

int rtsp_get_max_size()
{
	return s_body_max_size;
}

int rtsp_set_max_size(unsigned int bytes)
{
	s_body_max_size = bytes;
	return 0;
}

int rtsp_get_version(void* parser, int *major, int *minor)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	*major = ctx->vermajor;
	*minor = ctx->verminor;
	return 0;
}

int rtsp_get_status_code(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	assert(!is_server_mode(ctx));
	return ctx->u.reply.code;
}

const char* rtsp_get_status_reason(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	assert(!is_server_mode(ctx));
	return ctx->raw + ctx->u.reply.reason.pos;
}

const char* rtsp_get_request_method(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	assert(is_server_mode(ctx));
	return ctx->raw + ctx->u.req.method.pos;
}

const char* rtsp_get_request_uri(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	assert(is_server_mode(ctx));
	return ctx->raw + ctx->u.req.uri.pos;
}

const void* rtsp_get_content(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	assert(ctx->offset <= ctx->raw_size);
	return ctx->raw + ctx->offset;
}

int rtsp_get_header_count(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	return ctx->header_size;
}

int rtsp_get_header(void* parser, int idx, const char** name, const char** value)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);

	if(idx < 0 || idx >= ctx->header_size)
		return EINVAL;

	*name = ctx->raw + ctx->headers[idx].name.pos;
	*value = ctx->raw + ctx->headers[idx].value.pos;
	return 0;
}

const char* rtsp_get_header_by_name(void* parser, const char* name)
{
	int i;
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);

	for(i = 0; i < ctx->header_size; i++)
	{
		if(0 == strcasecmp(ctx->raw + ctx->headers[i].name.pos, name))
			return ctx->raw + ctx->headers[i].value.pos;
	}

	return NULL; // not found
}

int rtsp_get_header_by_name2(void* parser, const char* name, int *value)
{
	int i;
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);

	for(i = 0; i < ctx->header_size; i++)
	{
		if(0 == strcasecmp(ctx->raw + ctx->headers[i].name.pos, name))
		{
			*value = atoi(ctx->raw + ctx->headers[i].value.pos);
			return 0;
		}
	}

	return -1;
}

int rtsp_get_content_length(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	if(-1 == ctx->content_length)
	{
		assert(!is_server_mode(ctx));
		return ctx->raw_size - ctx->offset;
	}
	return ctx->content_length;
}

int rtsp_get_connection(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	return ctx->connection_close;
}

const char* rtsp_get_content_encoding(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	if(0 == ctx->content_encoding)
		return NULL;
	return ctx->raw + ctx->content_encoding;
}

const char* rtsp_get_transfer_encoding(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	if(0 == ctx->transfer_encoding)
		return NULL;
	return ctx->raw + ctx->transfer_encoding;
}

const char* rtsp_get_cookie(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	assert(!is_server_mode(ctx));
	if(0 == ctx->cookie)
		return NULL;
	return ctx->raw + ctx->cookie;
}

const char* rtsp_get_location(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	assert(!is_server_mode(ctx));
	if(0 == ctx->location)
		return NULL;
	return ctx->raw + ctx->location;
}
