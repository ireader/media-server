#include "rtsp-parser.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "cstringext.h"

#define KB (1024)
#define MB (1024*1024)

#define ISSPACE(c)		((c)==' ')
#define VMAX(a, b)		((a) > (b) ? (a) : (b))
#define VMIN(a, b)		((a) < (b) ? (a) : (b))

enum { SM_FIRSTLINE = 0, SM_HEADER = 100, SM_BODY = 200, SM_DONE = 300 };

struct rtsp_status_line
{
	int code;
	size_t reason_pos; // RTSP reason
	size_t reason_len;
};

struct rtsp_request_line
{
	char method[16];
	size_t uri_pos; // RTSP URI
	size_t uri_len;
};

struct rtsp_header
{
	size_t npos, nlen; // name
	size_t vpos, vlen; // value
};

struct rtsp_chunk
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

	struct rtsp_chunk chunk;

	// start line
	int verminor, vermajor;
	union
	{
		struct rtsp_request_line req;
		struct rtsp_status_line reply;
	};

	// headers
	struct rtsp_header *headers;
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

static inline void trim_right(const char* s, size_t *pos, size_t *len)
{
	//// left trim
	//while(*len > 0 && ISSPACE(s[*pos]))
	//{
	//	--*len;
	//	++*pos;
	//}

	// right trim
	while(*len > 0 && ISSPACE(s[*pos + *len - 1]))
	{
		--*len;
	}
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

static int rtsp_header_add(struct rtsp_context *ctx, struct rtsp_header* header)
{
	int size;
	struct rtsp_header *p;
	if(ctx->header_size+1 >= ctx->header_capacity)
	{
		size = ctx->header_capacity < 16 ? 16 : (ctx->header_size * 3 / 2);
		p = (struct rtsp_header*)realloc(ctx->headers, sizeof(struct rtsp_header) * size);
		if(!p)
			return ENOMEM;

		ctx->headers = p;
		ctx->header_capacity = size;
	}

	assert(header->npos > 0);
	assert(header->nlen > 0);
	assert(header->vpos > 0);
	assert(is_valid_token(ctx->raw+header->npos, header->nlen));
	ctx->raw[header->npos+header->nlen] = '\0';
	ctx->raw[header->vpos+header->vlen] = '\0';
	memmove(ctx->headers + ctx->header_size, header, sizeof(struct rtsp_header));
	++ctx->header_size;

	// handle
	rtsp_header_handler(ctx, header->npos, header->vpos);
	return 0;
}

// H5.1 Request-Line
// Request-Line = Method SP Request-URI SP RTSP-Version CRLF
// GET http://www.w3.org/pub/WWW/TheProject.html HTTP/1.1
static int rtsp_parse_request_line(struct rtsp_context *ctx)
{
	enum { 
		SM_REQUEST_METHOD = SM_FIRSTLINE, 
		SM_REQUEST_METHOD_SP,
		SM_REQUEST_URI,
		SM_REQUEST_VERSION, 
		SM_REQUEST_END
	};

	for(; ctx->offset < ctx->raw_size; ctx->offset++)
	{
		switch(ctx->stateM)
		{
		case SM_REQUEST_METHOD:
			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			if(' ' == ctx->raw[ctx->offset])
			{
				size_t n = VMIN(ctx->offset, sizeof(ctx->req.method));
				assert(ctx->offset < sizeof(ctx->req.method));
				strlcpy(ctx->req.method, ctx->raw, n);
				ctx->stateM = SM_REQUEST_METHOD_SP;
				assert(0 == ctx->req.uri_pos);
				assert(0 == ctx->req.uri_len);
			}
			break;

		case SM_REQUEST_METHOD_SP:
			if(ISSPACE(ctx->raw[ctx->offset]))
				break; // skip SP

			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			assert(0 == ctx->req.uri_pos);
			assert(0 == ctx->req.uri_len);
			ctx->req.uri_pos = ctx->offset;
			ctx->stateM = SM_REQUEST_URI;
			break;

			// H5.1.2 Request-URI
			// Request-URI = "*" | absoluteURI | abs_path | authority
		case SM_REQUEST_URI:
			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			if(' ' == ctx->raw[ctx->offset])
			{
				assert(0 == ctx->req.uri_len);
				ctx->req.uri_len = ctx->offset - ctx->req.uri_pos;
				ctx->raw[ctx->req.uri_pos + ctx->req.uri_len] = '\0';
				ctx->stateM = SM_REQUEST_VERSION;
			}
			else
			{
				// RFC3986 
				// 2.2 Reserved Characters
				// reserved    = gen-delims / sub-delims
				// gen-delims  = ":" / "/" / "?" / "#" / "[" / "]" / "@"
				// sub-delims  = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
				// 2.3.  Unreserved Characters
				// unreserved  = ALPHA / DIGIT / "-" / "." / "_" / "~"
				assert(isalnum(ctx->raw[ctx->offset]) || strchr("-._~:/?#[]@!$&'()*+,;=", ctx->raw[ctx->offset]) || '%' == ctx->raw[ctx->offset]);
			}
			break;

		case SM_REQUEST_VERSION:
			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			if(ctx->offset + 8 > ctx->raw_size)
				return 0; // wait for more data

			if(' ' == ctx->raw[ctx->offset])
				break; // skip SP

			// HTTP/1.1
			if(2 != sscanf(ctx->raw+ctx->offset, "RTSP/%1d.%1d",&ctx->vermajor, &ctx->verminor))
				return -1;

			assert(1 == ctx->vermajor);
			assert(1 == ctx->verminor || 0 == ctx->verminor);
			ctx->offset += 7; // skip
			ctx->stateM = SM_REQUEST_END;
			break;

		case SM_REQUEST_END:
			switch(ctx->raw[ctx->offset])
			{
			case ' ':
			case '\r':
				break;
			case '\n':
				++ctx->offset; // skip '\n'
				ctx->stateM = SM_HEADER;
				return 0;
			default:
				assert(0);
				return -1; // invalid
			}
			break;

		default:
			assert(0);
			return -1;
		}
	}

	return 0;
}

// H6.1 Status-Line
// Status-Line = RTSP-Version SP Status-Code SP Reason-Phrase CRLF
static int rtsp_parse_status_line(struct rtsp_context *ctx)
{
	int i;
	enum { 
		SM_STATUS_VERSION = SM_FIRSTLINE, 
		SM_STATUS_CODE, 
		SM_STATUS_CODE_SP, 
		SM_STATUS_REASON 
	};

	for(; ctx->offset < ctx->raw_size; ctx->offset++)
	{
		switch(ctx->stateM)
		{
		case SM_STATUS_VERSION:
			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			if(ctx->offset + 8 > ctx->raw_size)
				return 0; // wait for more data

			assert(0 == ctx->offset);
			if(2 != sscanf(ctx->raw+ctx->offset, "RTSP/%1d.%1d",&ctx->vermajor, &ctx->verminor))
				return -1;

			assert(1 == ctx->vermajor);
			assert(1 == ctx->verminor || 0 == ctx->verminor);
			ctx->offset += 7; // skip
			ctx->stateM = SM_STATUS_CODE;
			break;

		case SM_STATUS_CODE:
			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			if(' ' == ctx->raw[ctx->offset])
				break; // skip SP

			if('0' > ctx->raw[ctx->offset] || ctx->raw[ctx->offset] > '9')
				return -1; // invalid

			if(ctx->offset + 3 > ctx->raw_size)
				return 0; // wait for more data

			assert(0 == ctx->reply.code);
			for(i = 0; i < 3; i++)
				ctx->reply.code = ctx->reply.code * 10 + (ctx->raw[ctx->offset+i] - '0');

			ctx->offset += 2; // skip
			ctx->stateM = SM_STATUS_CODE_SP;
			break;

		case SM_STATUS_CODE_SP:
			assert('\r' != ctx->raw[ctx->offset]);
			assert('\n' != ctx->raw[ctx->offset]);
			if(ISSPACE(ctx->raw[ctx->offset]))
				break; // skip SP

			assert(0 == ctx->reply.reason_pos);
			assert(0 == ctx->reply.reason_len);
			ctx->reply.reason_pos = ctx->offset;
			ctx->stateM = SM_STATUS_REASON;
			break;

		case SM_STATUS_REASON:
			switch(ctx->raw[ctx->offset])
			{
			//case '\r':
			//	break;
			case '\n':
				assert('\r' == ctx->raw[ctx->offset-1]);
				ctx->reply.reason_len = ctx->offset - 1 - ctx->reply.reason_pos;
				trim_right(ctx->raw, &ctx->reply.reason_pos, &ctx->reply.reason_len);
				ctx->raw[ctx->reply.reason_pos + ctx->reply.reason_len] = '\0';
				ctx->stateM = SM_HEADER;
				++ctx->offset; // skip \n
				return 0;

			default:
				break;
			}
			break;

		default:
			assert(0);
			return -1;
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
		SM_HEADER_NAME_SP,
		SM_HEADER_SEPARATOR,
		SM_HEADER_VALUE
	};

	int r;
	struct rtsp_header header;
	memset(&header, 0, sizeof(struct rtsp_header)); // init header

	for(; ctx->offset < ctx->raw_size; ctx->offset++)
	{
		switch(ctx->stateM)
		{
		case SM_HEADER_START:
			switch(ctx->raw[ctx->offset])
			{
			case '\r':
				if(ctx->offset + 2 > ctx->raw_size)
					return 0; // wait more date

				++ctx->offset;
				assert('\n' == ctx->raw[ctx->offset]);

			case '\n':
				++ctx->offset;
				ctx->stateM = SM_BODY;
				return 0;

			case ' ':
			case '\t':
				assert(0); // multi-line header ?
				break;

			default:
				assert(0 == header.npos);
				assert(0 == header.nlen);
				header.npos = ctx->offset;
				ctx->stateM = SM_HEADER_NAME;
			}
			break;

		case SM_HEADER_NAME:
			switch(ctx->raw[ctx->offset])
			{
			case '\r':
			case '\n':
				assert(0);
				return -1; // invalid

			case ' ':
				header.nlen = ctx->offset - header.npos;
				assert(header.nlen > 0 && is_valid_token(ctx->raw+header.npos, header.nlen));
				ctx->stateM = SM_HEADER_NAME_SP;
				break;

			case ':':
				header.nlen = ctx->offset - header.npos;
				assert(header.nlen > 0 && is_valid_token(ctx->raw+header.npos, header.nlen));
				ctx->stateM = SM_HEADER_SEPARATOR;
				break;
			}
			break;

		case SM_HEADER_NAME_SP:
			switch(ctx->raw[ctx->offset])
			{
			case ' ':
				break; // skip SP

			case ':':
				ctx->stateM = SM_HEADER_SEPARATOR;
				break;

			default:
				assert(0);
				return -1;
			}
			break;

		case SM_HEADER_SEPARATOR:
			switch(ctx->raw[ctx->offset])
			{
			case '\r':
			case '\n':
				assert(0);
				return -1; // invalid

			case ' ':
				break; // skip SP

			default:
				ctx->stateM = SM_HEADER_VALUE;
				header.vpos = ctx->offset;
				break;
			}
			break;

		case SM_HEADER_VALUE:
			switch(ctx->raw[ctx->offset])
			{
			case '\n':
				assert('\r' == ctx->raw[ctx->offset-1]);
				header.vlen = ctx->offset - 1 - header.vpos;
				trim_right(ctx->raw, &header.vpos, &header.vlen);
				ctx->stateM = SM_HEADER;

				// add new header
				r = rtsp_header_add(ctx, &header);
				if(0 != r)
					return r;
				memset(&header, 0, sizeof(struct rtsp_header)); // reuse header
				break;

			default:
				break;
			}
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

	memset(&ctx->req, 0, sizeof(ctx->req));
	memset(&ctx->reply, 0, sizeof(ctx->reply));
	memset(&ctx->chunk, 0, sizeof(struct rtsp_chunk));
	ctx->stateM = SM_FIRSTLINE;
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

	if(SM_FIRSTLINE <= ctx->stateM && ctx->stateM < SM_HEADER)
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
	return ctx->reply.code;
}

const char* rtsp_get_status_reason(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	assert(!is_server_mode(ctx));
	return ctx->raw + ctx->reply.reason_pos;
}

const char* rtsp_get_request_method(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	assert(is_server_mode(ctx));
	return ctx->req.method;
}

const char* rtsp_get_request_uri(void* parser)
{
	struct rtsp_context *ctx;
	ctx = (struct rtsp_context*)parser;
	assert(ctx->stateM>=SM_BODY);
	assert(is_server_mode(ctx));
	return ctx->raw + ctx->req.uri_pos;
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

	*name = ctx->raw + ctx->headers[idx].npos;
	*value = ctx->raw + ctx->headers[idx].vpos;
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
		if(0 == strcasecmp(ctx->raw + ctx->headers[i].npos, name))
			return ctx->raw + ctx->headers[i].vpos;
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
		if(0 == strcasecmp(ctx->raw + ctx->headers[i].npos, name))
		{
			*value = atoi(ctx->raw + ctx->headers[i].vpos);
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
