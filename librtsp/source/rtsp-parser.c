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

struct rtsp_parser_t
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

static inline int is_server_mode(struct rtsp_parser_t *rtsp)
{
	return RTSP_PARSER_SERVER==rtsp->server_mode ? 1 : 0;
}

static inline int is_transfer_encoding_chunked(struct rtsp_parser_t *rtsp)
{
	return (rtsp->transfer_encoding>0 && 0==strncasecmp("chunked", rtsp->raw+rtsp->transfer_encoding, 7)) ? 1 : 0;
}

static int rtsp_rawdata(struct rtsp_parser_t *rtsp, const void* data, int bytes)
{
	void *p;
	int capacity;

	if(rtsp->raw_capacity - rtsp->raw_size < (size_t)bytes + 1)
	{
		capacity = (rtsp->raw_capacity > 4*MB) ? 50*MB : (rtsp->raw_capacity > 16*KB ? 2*MB : 8*KB);
		capacity = (bytes + 1) > capacity ? (bytes + 1) : capacity;
		p = realloc(rtsp->raw, rtsp->raw_capacity + capacity);
		if(!p)
			return ENOMEM;

		rtsp->raw_capacity += capacity;
		rtsp->raw = p;
	}

	assert(rtsp->raw_capacity - rtsp->raw_size > (size_t)bytes+1);
	memmove((char*)rtsp->raw + rtsp->raw_size, data, bytes);
	rtsp->raw_size += bytes;
	rtsp->raw[rtsp->raw_size] = '\0'; // auto add ending '\0'
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
static int rtsp_header_handler(struct rtsp_parser_t *rtsp, size_t npos, size_t vpos)
{
	const char* name = rtsp->raw + npos;
	const char* value = rtsp->raw + vpos;

	if(0 == strcasecmp("Content-Length", name))
	{
		// H4.4 Message Length, section 3, ignore content-length if in chunked mode
		if(is_transfer_encoding_chunked(rtsp))
			rtsp->content_length = -1;
		else
			rtsp->content_length = atoi(value);
		assert(rtsp->content_length >= 0 && (0==s_body_max_size || rtsp->content_length < (int)s_body_max_size));
	}
	else if(0 == strcasecmp("Connection", name))
	{
		rtsp->connection_close = (0 == strcasecmp("close", value)) ? 1 : 0;
	}
	else if(0 == strcasecmp("Content-Encoding", name))
	{
		// gzip/compress/deflate/identity(default)
		rtsp->content_encoding = (int)vpos;
	}
	else if(0 == strcasecmp("Transfer-Encoding", name))
	{
		rtsp->transfer_encoding = (int)vpos;
		if(0 == strncasecmp("chunked", value, 7))
		{
			// chunked can't use with content-length
			// H4.4 Message Length, section 3,
			assert(-1 == rtsp->content_length);
			rtsp->raw[rtsp->transfer_encoding + 7] = '\0'; // ignore parameters
		}
	}
	else if(0 == strcasecmp("Set-Cookie", name))
	{
		rtsp->cookie = (int)vpos;
	}
	else if(0 == strcasecmp("Location", name))
	{
		rtsp->location = (int)vpos;
	}

	return 0;
}

static int rtsp_header_add(struct rtsp_parser_t *rtsp, struct rtsp_header_t* header)
{
	int size;
	struct rtsp_header_t *p;
	if(rtsp->header_size+1 >= rtsp->header_capacity)
	{
		size = rtsp->header_capacity < 16 ? 16 : (rtsp->header_size * 3 / 2);
		p = (struct rtsp_header_t*)realloc(rtsp->headers, sizeof(struct rtsp_header_t) * size);
		if(!p)
			return ENOMEM;

		rtsp->headers = p;
		rtsp->header_capacity = size;
	}

	assert(header->name.pos > 0);
	assert(header->name.len > 0);
	assert(header->value.pos > 0);
	assert(is_valid_token(rtsp->raw + header->name.pos, header->name.len));
	rtsp->raw[header->name.pos + header->name.len] = '\0';
	rtsp->raw[header->value.pos + header->value.len] = '\0';
	memcpy(rtsp->headers + rtsp->header_size, header, sizeof(struct rtsp_header_t));
	++rtsp->header_size;
	return 0;
}

// H5.1 Request-Line
// Request-Line = Method SP Request-URI SP RTSP-Version CRLF
// GET http://www.w3.org/pub/WWW/TheProject.html HTTP/1.1
static int rtsp_parse_request_line(struct rtsp_parser_t *rtsp)
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
	v[0] = &rtsp->u.req.method.pos;
	v[1] = &rtsp->u.req.method.len;
	v[2] = &rtsp->u.req.uri.pos;
	v[3] = &rtsp->u.req.uri.len;
	v[4] = &rtsp->header.name.pos; // version
	v[5] = &rtsp->header.name.len;

	for (; rtsp->offset < rtsp->raw_size; ++rtsp->offset)
	{
		c = rtsp->raw[rtsp->offset];
		switch (rtsp->stateM)
		{
		case SM_REQUEST_START:
		case SM_REQUEST_SP1:
		case SM_REQUEST_SP2:
			if (' ' != c && '\t' != c)
			{
				*(v[rtsp->stateM - SM_REQUEST_START]) = rtsp->offset;
				rtsp->stateM += 1; // next state
				rtsp->offset -= 1; // go back
			}
			break;

		case SM_REQUEST_METHOD:
		case SM_REQUEST_URI:
		case SM_REQUEST_VERSION:
			if (' ' == c || '\t' == c || '\r' == c || '\n' == c)
			{
				*(v[rtsp->stateM - SM_REQUEST_START]) = rtsp->offset - *(v[rtsp->stateM - SM_REQUEST_START - 1]);
				rtsp->stateM += 1; // next state
				rtsp->offset -= 1; // go back
			}
			break;

		case SM_REQUEST_SP3:
			switch (c)
			{
			case ' ':
			case '\t':
				break; // continue

			case '\r':
				rtsp->stateM = SM_REQUEST_CR;
				break;

			case '\n':
				rtsp->stateM = SM_REQUEST_LF;
				rtsp->offset -= 1; // go back
				break;

			default:
				rtsp->stateM = SM_REQUEST_VERSION;
				break;
			}
			break;

		case SM_REQUEST_CR:
			if ('\n' != c)
			{
				assert(0);
				return -1;
			}
			rtsp->stateM = SM_REQUEST_LF;
			rtsp->offset -= 1; // go back
			break;

		case SM_REQUEST_LF:
			assert('\n' == c);
			// H5.1.1 Method (p24)
			// Method = OPTIONS | GET | HEAD | POST | PUT | DELETE | TRACE | CONNECT | extension-method
			if (rtsp->u.req.method.len < 1
				// H5.1.2 Request-URI (p24)
				// Request-URI = "*" | absoluteURI | abs_path | authority
				|| rtsp->u.req.uri.len < 1
				// H3.1 HTTP Version (p13)
				// HTTP-Version = "HTTP" "/" 1*DIGIT "." 1*DIGIT
				|| rtsp->header.name.len < 8
				|| 2 != sscanf(rtsp->raw + rtsp->header.name.pos, "RTSP/%d.%d", &rtsp->vermajor, &rtsp->verminor))
			{
				assert(0);
				return -1;
			}

			rtsp->raw[rtsp->u.req.method.pos + rtsp->u.req.method.len] = '\0';
			rtsp->raw[rtsp->u.req.uri.pos + rtsp->u.req.uri.len] = '\0';
			assert(1 == rtsp->vermajor || 2 == rtsp->vermajor);
			assert(1 == rtsp->verminor || 0 == rtsp->verminor);
			rtsp->stateM = SM_HEADER;
			rtsp->offset += 1; // skip '\n'
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
static int rtsp_parse_status_line(struct rtsp_parser_t *rtsp)
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
	v[0] = &rtsp->header.name.pos; // version
	v[1] = &rtsp->header.name.len;
	v[2] = &rtsp->header.value.pos; // status code
	v[3] = &rtsp->header.value.len;
	v[4] = &rtsp->u.reply.reason.pos;
	v[5] = &rtsp->u.reply.reason.len;

	for (; rtsp->offset < rtsp->raw_size; rtsp->offset++)
	{
		c = rtsp->raw[rtsp->offset];
		switch (rtsp->stateM)
		{
		case SM_STATUS_START:
		case SM_STATUS_SP1:
		case SM_STATUS_SP2:
			if (' ' != c && '\t' != c)
			{
				*(v[rtsp->stateM - SM_STATUS_START]) = rtsp->offset;
				rtsp->stateM += 1; // next state
				rtsp->offset -= 1; // go back
			}
			break;

		case SM_STATUS_VERSION:
		case SM_STATUS_CODE:
		case SM_STATUS_REASON:
			if (' ' == c || '\t' == c || '\r' == c || '\n' == c)
			{
				*(v[rtsp->stateM - SM_STATUS_START]) = rtsp->offset - *(v[rtsp->stateM - SM_STATUS_START - 1]);
				rtsp->stateM += 1; // next state
				rtsp->offset -= 1; // go back
			}
			break;

		case SM_STATUS_SP3:
			switch (c)
			{
			case ' ':
			case '\t':
				break; // continue

			case '\r':
				rtsp->stateM = SM_STATUS_CR;
				break;

			case '\n':
				rtsp->stateM = SM_STATUS_LF;
				rtsp->offset -= 1; // go back
				break;

			default:
				rtsp->stateM = SM_STATUS_REASON;
				break;
			}
			break;

		case SM_STATUS_CR:
			if ('\n' != c)
			{
				assert(0);
				return -1;
			}
			rtsp->stateM = SM_STATUS_LF;
			rtsp->offset -= 1; // go back
			break;

		case SM_STATUS_LF:
			assert('\n' == c);
			// H3.1 HTTP Version (p13)
			// HTTP-Version = "HTTP" "/" 1*DIGIT "." 1*DIGIT
			if (rtsp->header.name.len < 8 || 2 != sscanf(rtsp->raw + rtsp->header.name.pos, "RTSP/%d.%d", &rtsp->vermajor, &rtsp->verminor)
				// H6.1.1 Status Code and Reason Phrase (p26)
				// The Status-Code element is a 3-digit integer result code
				|| rtsp->header.value.len != 3 || rtsp->u.reply.reason.len < 1)
			{
				assert(0);
				return -1;
			}

			rtsp->raw[rtsp->u.reply.reason.pos + rtsp->u.reply.reason.len] = '\0';
			rtsp->raw[rtsp->header.value.pos + rtsp->header.value.len] = '\0';
			rtsp->u.reply.code = atoi(rtsp->raw + rtsp->header.value.pos);
			assert(1 == rtsp->vermajor || 2 == rtsp->vermajor);
			assert(1 == rtsp->verminor || 0 == rtsp->verminor);
			rtsp->stateM = SM_HEADER;
			rtsp->offset += 1; // skip '\n'
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
static int rtsp_parse_header_line(struct rtsp_parser_t *rtsp)
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

	v[0] = &rtsp->header.name.pos;
	v[1] = &rtsp->header.name.len;
	v[2] = &dummy;
	v[3] = &dummy;
	v[4] = &rtsp->header.value.pos;
	v[5] = &rtsp->header.value.len;

	for (; rtsp->offset < rtsp->raw_size; rtsp->offset++)
	{
		c = rtsp->raw[rtsp->offset];
		switch (rtsp->stateM)
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
				rtsp->header.name.pos = 0; // use for multiple lines flag
				rtsp->stateM = SM_HEADER_SP2; // next state
				break;

			case '\r':
				rtsp->header.value.pos = 0; // use for header end flag
				rtsp->stateM = SM_HEADER_CR;
				break;

			case '\n':
				rtsp->header.value.pos = 0; // use for header end flag
				rtsp->stateM = SM_HEADER_LF;
				rtsp->offset -= 1; // go back
				break;

			default:
				rtsp->header.name.pos = rtsp->offset;
				rtsp->stateM += 1; // next state
				rtsp->offset -= 1; // go back
				break;
			}
			break;

		case SM_HEADER_SP1:
		case SM_HEADER_SP2:
			if (' ' != c && '\t' != c)
			{
				*(v[rtsp->stateM - SM_HEADER_START]) = rtsp->offset;
				rtsp->stateM += 1; // next state
				rtsp->offset -= 1; // go back
			}
			break;

		case SM_HEADER_NAME:
		case SM_HEADER_VALUE:
			if (' ' == c || '\t' == c || '\r' == c || '\n' == c || (':' == c && SM_HEADER_NAME == rtsp->stateM))
			{
				*(v[rtsp->stateM - SM_HEADER_START]) = rtsp->offset - *(v[rtsp->stateM - SM_HEADER_START - 1]);
				rtsp->stateM += 1; // next state
				rtsp->offset -= 1; // go back
			}
			break;

		case SM_HEADER_SEPARATOR:
			if (':' != c)
			{
				assert(0);
				return -1;
			}

			// accept
			rtsp->stateM += 1; // next state
			break;

		case SM_HEADER_SP3:
			switch (c)
			{
			case ' ':
			case '\t':
				break; // continue

			case '\r':
				rtsp->stateM = SM_HEADER_CR;
				break;

			case '\n':
				rtsp->stateM = SM_HEADER_LF;
				rtsp->offset -= 1; // go back
				break;

			default:
				rtsp->stateM = SM_HEADER_VALUE;
				break;
			}
			break;

		case SM_HEADER_CR:
			if ('\n' != c)
			{
				assert(0);
				return -1;
			}
			rtsp->stateM = SM_HEADER_LF;
			rtsp->offset -= 1; // go back
			break;

		case SM_HEADER_LF:
			assert('\n' == c);
			if (0 == rtsp->header.value.pos)
			{
				rtsp->stateM = SM_BODY;
				rtsp->offset += 1; // skip '\n'
				return 0;
			}

			if (0 == rtsp->header.name.pos)
			{
				// multiple lines header
				size_t i;
				struct rtsp_header_t* h;

				if (rtsp->header_size < 1)
				{
					assert(0);
					return -1;
				}

				h = &rtsp->headers[rtsp->header_size - 1];
				for (i = h->value.len; i < rtsp->header.value.pos; i++)
				{
					rtsp->raw[i] = ' '; // replace with SP
				}
				h->value.len = rtsp->header.value.pos - h->value.pos + rtsp->header.value.len;
				rtsp->raw[h->value.pos + h->value.len] = '\0';

				rtsp_header_handler(rtsp, h->name.pos, h->value.pos); // handle
			}
			else
			{
				if (rtsp->header.name.len < 1)
				{
					assert(0);
					return -1;
				}

				if (0 != rtsp_header_add(rtsp, &rtsp->header))
					return -1;

				rtsp_header_handler(rtsp, rtsp->header.name.pos, rtsp->header.value.pos); // handle
			}

			rtsp->stateM = SM_HEADER; // continue
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
static int rtsp_parse_chunked(struct rtsp_parser_t *rtsp)
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
	assert(is_transfer_encoding_chunked(rtsp));
	if(0 == rtsp->chunk.offset)
	{
		rtsp->chunk.offset = rtsp->offset;
		assert(-1 == rtsp->content_length);
		rtsp->content_length = 0;
	}

	for(; rtsp->chunk.offset < rtsp->raw_size; rtsp->chunk.offset++)
	{
		c = rtsp->raw[rtsp->chunk.offset];

		switch(rtsp->stateM)
		{
		case CHUNK_START:
			assert(0 == rtsp->chunk.len);
			if('0' <= c && c <= '9')
			{
				rtsp->chunk.len = c - '0';
			}
			else if('a' <= c && c <= 'f')
			{
				rtsp->chunk.len = c - 'a' + 10;
			}
			else if('A' <= c && c <= 'F')
			{
				rtsp->chunk.len = c - 'A' + 10;
			}
			else
			{
				assert(0);
				return -1;
			}

			rtsp->stateM = CHUNK_SIZE;
			break;

		case CHUNK_SIZE:
			if('0' <= c && c <= '9')
			{
				rtsp->chunk.len = rtsp->chunk.len * 16 + (c - '0');
			}
			else if('a' <= c && c <= 'f')
			{
				rtsp->chunk.len = rtsp->chunk.len * 16 + (c - 'a' + 10);
			}
			else if('A' <= c && c <= 'F')
			{
				rtsp->chunk.len = rtsp->chunk.len * 16 + (c - 'A' + 10);
			}
			else
			{
				switch(c)
				{
				case '\t':
				case ' ':
				case ';':
					rtsp->stateM = CHUNK_EXTENSION;
					break;

				case '\r':
					rtsp->stateM = CHUNK_EXTENSION_CR;
					break;

				case '\n':
					rtsp->chunk.pos = rtsp->chunk.offset + 1;
					rtsp->stateM = 0==rtsp->chunk.len ? CHUNK_TRAILER_START : CHUNK_DATA;
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
				rtsp->stateM = CHUNK_EXTENSION_CR;
				break;

			case '\n':
				rtsp->chunk.pos = rtsp->chunk.offset + 1;
				rtsp->stateM = 0==rtsp->chunk.len ? CHUNK_TRAILER_START : CHUNK_DATA;
				break;
			}
			break;

		case CHUNK_EXTENSION_CR:
			if('\n' != c)
			{
				assert(0);
				return -1;
			}

			rtsp->chunk.pos = rtsp->chunk.offset + 1;
			rtsp->stateM = 0==rtsp->chunk.len ? CHUNK_TRAILER_START : CHUNK_DATA;
			break;

		case CHUNK_DATA:
			assert(rtsp->chunk.len > 0);
			assert(0 != rtsp->chunk.pos);
			if(rtsp->chunk.pos + rtsp->chunk.len + 2 > rtsp->raw_size)
				return 0; // wait for more data

			if('\r' != rtsp->raw[rtsp->chunk.pos + rtsp->chunk.len] || '\n' != rtsp->raw[rtsp->chunk.pos + rtsp->chunk.len + 1])
			{
				assert(0);
				return -1;
			}

			memmove(rtsp->raw+rtsp->offset+rtsp->content_length, rtsp->raw+rtsp->chunk.pos, rtsp->chunk.len);
			rtsp->raw[rtsp->offset+rtsp->content_length+rtsp->chunk.len] = '\0';
			rtsp->content_length += rtsp->chunk.len;
			rtsp->stateM = CHUNK_START;

			rtsp->chunk.offset += rtsp->chunk.len + 1; // skip \r\n
			rtsp->chunk.pos = rtsp->chunk.len = 0; // reuse chunk
			break;

		case CHUNK_TRAILER_START:
			switch(c)
			{
			case '\r':
				rtsp->stateM = CHUNK_END_CR;
				break;

			case '\n':
				++rtsp->chunk.offset;
				rtsp->stateM = SM_DONE;
				return 0;

			default:
				rtsp->stateM = CHUNK_TRAILER;
				break;
			}
			break;

		case CHUNK_TRAILER:
			switch(c)
			{
			case '\r':
				rtsp->stateM = CHUNK_TRAILER_CR;
				break;

			case '\n':
				rtsp->stateM = CHUNK_TRAILER_START;
				break;
			}
			break;

		case CHUNK_TRAILER_CR:
			if('\n' != c)
			{
				assert(0);
				return -1;
			}
			rtsp->stateM = CHUNK_TRAILER_START;
			break;

		case CHUNK_END:
			switch(c)
			{
			case '\r':
				rtsp->stateM = CHUNK_END_CR;
				break;

			case '\n':
				++rtsp->chunk.offset;
				rtsp->stateM = SM_DONE;
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
			rtsp->stateM = SM_DONE;
			++rtsp->chunk.offset;
			return 0;
		}
	}

	return 0;
}

struct rtsp_parser_t* rtsp_parser_create(enum RTSP_PARSER_MODE mode)
{
	struct rtsp_parser_t *rtsp;
	rtsp = (struct rtsp_parser_t*)malloc(sizeof(struct rtsp_parser_t));
	if(!rtsp)
		return NULL;

	memset(rtsp, 0, sizeof(struct rtsp_parser_t));
	rtsp->server_mode = mode;
	rtsp_parser_clear(rtsp);
	return rtsp;
}

int rtsp_parser_destroy(struct rtsp_parser_t *rtsp)
{
	if(rtsp->raw)
	{
		assert(rtsp->raw_capacity > 0);
		free(rtsp->raw);
		rtsp->raw = 0;
		rtsp->raw_size = 0;
		rtsp->raw_capacity = 0;
	}

	if(rtsp->headers)
	{
		assert(rtsp->header_capacity > 0);
		free(rtsp->headers);
		rtsp->headers = 0;
		rtsp->header_size = 0;
		rtsp->header_capacity = 0;
	}

	free(rtsp);
	return 0;
}

void rtsp_parser_clear(struct rtsp_parser_t *rtsp)
{
	memset(&rtsp->u.req, 0, sizeof(rtsp->u.req));
	memset(&rtsp->u.reply, 0, sizeof(rtsp->u.reply));
	memset(&rtsp->chunk, 0, sizeof(struct rtsp_chunk_t));
	rtsp->stateM = SM_START_LINE;
	rtsp->offset = 0;
	rtsp->raw_size = 0;
	rtsp->header_size = 0;
	rtsp->content_length = -1;
	rtsp->connection_close = -1;
	rtsp->content_encoding = 0;
	rtsp->transfer_encoding = 0;
	rtsp->cookie = 0;
	rtsp->location = 0;
}

int rtsp_parser_input(struct rtsp_parser_t *rtsp, const void* data, int *bytes)
{
	enum { INPUT_NEEDMORE = 1, INPUT_DONE = 0, };

	int r;
	// save raw data
	r = rtsp_rawdata(rtsp, data, *bytes);
	if(0 != r)
	{
		assert(r < 0);
		return r;
	}

	if(SM_START_LINE <= rtsp->stateM && rtsp->stateM < SM_HEADER)
	{
		r = is_server_mode(rtsp) ? rtsp_parse_request_line(rtsp) : rtsp_parse_status_line(rtsp);
	}

	if(SM_HEADER <= rtsp->stateM && rtsp->stateM < SM_BODY)
	{
		r = rtsp_parse_header_line(rtsp);
	}

	assert(r <= 0);
	if(SM_BODY <= rtsp->stateM && rtsp->stateM < SM_DONE)
	{
		if(is_transfer_encoding_chunked(rtsp))
		{
			r = rtsp_parse_chunked(rtsp);
		}
		else
		{
			if(-1 == rtsp->content_length)
			{
				// RTSP2326 4.4 4.4 Message Length
				rtsp->content_length = 0;
				rtsp->stateM = SM_DONE;
			}
			else
			{
				assert(rtsp->raw_size <= rtsp->offset + rtsp->content_length);
				if(rtsp->raw_size >= rtsp->offset + rtsp->content_length)
					rtsp->stateM = SM_DONE;
			}
		}
	}

	if(r < 0)
		return r;

	*bytes = 0;
	return rtsp->stateM == SM_DONE ? INPUT_DONE : INPUT_NEEDMORE;
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

int rtsp_get_version(struct rtsp_parser_t *rtsp, int *major, int *minor)
{
	assert(rtsp->stateM>=SM_BODY);
	*major = rtsp->vermajor;
	*minor = rtsp->verminor;
	return 0;
}

int rtsp_get_status_code(struct rtsp_parser_t *rtsp)
{
	assert(rtsp->stateM>=SM_BODY);
	assert(!is_server_mode(rtsp));
	return rtsp->u.reply.code;
}

const char* rtsp_get_status_reason(struct rtsp_parser_t *rtsp)
{
	assert(rtsp->stateM>=SM_BODY);
	assert(!is_server_mode(rtsp));
	return rtsp->raw + rtsp->u.reply.reason.pos;
}

const char* rtsp_get_request_method(struct rtsp_parser_t *rtsp)
{
	assert(rtsp->stateM>=SM_BODY);
	assert(is_server_mode(rtsp));
	return rtsp->raw + rtsp->u.req.method.pos;
}

const char* rtsp_get_request_uri(struct rtsp_parser_t *rtsp)
{
	assert(rtsp->stateM>=SM_BODY);
	assert(is_server_mode(rtsp));
	return rtsp->raw + rtsp->u.req.uri.pos;
}

const void* rtsp_get_content(struct rtsp_parser_t *rtsp)
{
	assert(rtsp->stateM>=SM_BODY);
	assert(rtsp->offset <= rtsp->raw_size);
	return rtsp->raw + rtsp->offset;
}

int rtsp_get_header_count(struct rtsp_parser_t *rtsp)
{
	assert(rtsp->stateM>=SM_BODY);
	return rtsp->header_size;
}

int rtsp_get_header(struct rtsp_parser_t *rtsp, int idx, const char** name, const char** value)
{
	assert(rtsp->stateM>=SM_BODY);

	if(idx < 0 || idx >= rtsp->header_size)
		return EINVAL;

	*name = rtsp->raw + rtsp->headers[idx].name.pos;
	*value = rtsp->raw + rtsp->headers[idx].value.pos;
	return 0;
}

const char* rtsp_get_header_by_name(struct rtsp_parser_t *rtsp, const char* name)
{
	int i;
	assert(rtsp->stateM>=SM_BODY);

	for(i = 0; i < rtsp->header_size; i++)
	{
		if(0 == strcasecmp(rtsp->raw + rtsp->headers[i].name.pos, name))
			return rtsp->raw + rtsp->headers[i].value.pos;
	}

	return NULL; // not found
}

int rtsp_get_header_by_name2(struct rtsp_parser_t *rtsp, const char* name, int *value)
{
	int i;
	assert(rtsp->stateM>=SM_BODY);

	for(i = 0; i < rtsp->header_size; i++)
	{
		if(0 == strcasecmp(rtsp->raw + rtsp->headers[i].name.pos, name))
		{
			*value = atoi(rtsp->raw + rtsp->headers[i].value.pos);
			return 0;
		}
	}

	return -1;
}

int rtsp_get_content_length(struct rtsp_parser_t *rtsp)
{
	assert(rtsp->stateM>=SM_BODY);
	if(-1 == rtsp->content_length)
	{
		assert(!is_server_mode(rtsp));
		return rtsp->raw_size - rtsp->offset;
	}
	return rtsp->content_length;
}

int rtsp_get_connection(struct rtsp_parser_t *rtsp)
{
	assert(rtsp->stateM>=SM_BODY);
	return rtsp->connection_close;
}

const char* rtsp_get_content_encoding(struct rtsp_parser_t *rtsp)
{
	assert(rtsp->stateM>=SM_BODY);
	if(0 == rtsp->content_encoding)
		return NULL;
	return rtsp->raw + rtsp->content_encoding;
}

const char* rtsp_get_transfer_encoding(struct rtsp_parser_t *rtsp)
{
	assert(rtsp->stateM>=SM_BODY);
	if(0 == rtsp->transfer_encoding)
		return NULL;
	return rtsp->raw + rtsp->transfer_encoding;
}

const char* rtsp_get_cookie(struct rtsp_parser_t *rtsp)
{
	assert(rtsp->stateM>=SM_BODY);
	assert(!is_server_mode(rtsp));
	if(0 == rtsp->cookie)
		return NULL;
	return rtsp->raw + rtsp->cookie;
}

const char* rtsp_get_location(struct rtsp_parser_t *rtsp)
{
	assert(rtsp->stateM>=SM_BODY);
	assert(!is_server_mode(rtsp));
	if(0 == rtsp->location)
		return NULL;
	return rtsp->raw + rtsp->location;
}
