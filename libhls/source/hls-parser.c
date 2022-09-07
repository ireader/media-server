// https://tools.ietf.org/html/rfc8216
// https://developer.apple.com/documentation/http_live_streaming/about_the_ext-x-version_tag
//
// audio/mpegurl
// application/vnd.apple.mpegurl

#include "hls-parser.h"
#include "hls-string.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct hls_parser_t
{
	size_t media_capacity;
	size_t variant_capacity;
	size_t session_key_capacity;
	size_t session_data_capacity;
	struct hls_variant_t* variant;
	struct hls_master_t* master;

	size_t segment_capacity;
	struct hls_segment_t* segment;
	struct hls_segment_t* key_segment; // last segment has key
	struct hls_playlist_t* playlist;
};

enum 
{ 
	ATTR_VALUE_TYPE_UINT32, 
	ATTR_VALUE_TYPE_UINT64, 
	ATTR_VALUE_TYPE_FLOAT32, 
	ATTR_VALUE_TYPE_FLOAT64, 
	ATTR_VALUE_TYPE_STRING, 
	ATTR_VALUE_TYPE_STRING_BOOL,
};

struct hls_tag_attr_t
{
	int cls;
	const char* name;
	void* ptr;
};

#define HLS_TAG_ATTR_VALUE(a, cls0, name0, ptr0) {a.cls=cls0; a.name=name0; a.ptr=ptr0; }

static int hls_parser_realloc(void** ptr, size_t* capacity, size_t len, size_t incr, size_t size);

static int hls_attr_read(const char* value, size_t n, int cls, void* ptr)
{
	switch (cls)
	{
	case ATTR_VALUE_TYPE_STRING_BOOL:
		*(int*)ptr = (3 == n && 0 == strncasecmp(value, "YES", 3)) ? 1 : 0;
		return 0;

	case ATTR_VALUE_TYPE_UINT32:
		*(uint32_t*)ptr = (uint32_t)strtoul(value, NULL, 10);
		break;

	case ATTR_VALUE_TYPE_UINT64:
		*(uint64_t*)ptr = (uint64_t)strtoull(value, NULL, 10);
		break;

	case ATTR_VALUE_TYPE_FLOAT32:
		*(float*)ptr = (float)strtod(value, NULL);
		break;

	case ATTR_VALUE_TYPE_FLOAT64:
		*(double*)ptr = strtod(value, NULL);
		break;

	case ATTR_VALUE_TYPE_STRING:
		*((char**)ptr) = (char*)value;
		((char*)value)[n] = 0;
		break;

	default:
		assert(0);
		return -1;
	}

	return 0;
}

static int hls_parse_attrs(const char* data, size_t bytes, struct hls_tag_attr_t* attrs, size_t nattrs)
{
	int r;
	size_t i, n, nn, nv;
	const char* ptr, *next;
	const char* name, *value;

	r = 0;
	for (ptr = data; ptr && ptr < data + bytes && 0 == r; ptr = next)
	{
		n = hls_strsplit(ptr, data + bytes, ",", "\"", &next);

		nn = hls_strsplit(ptr, ptr + n, "=", "", &value);
		name = hls_strtrim(ptr, &nn, " \t", " \t"); // trim SP/HTAB
		nv = ptr + n - value;
		value = hls_strtrim(value, &nv, " \t'\"", " \t'\""); // trim SP/HTAB/'/"

		for (i = 0; i < nattrs; i++)
		{
			if (nn == strlen(attrs[i].name) && 0 == strncasecmp(attrs[i].name, name, nn))
			{
				r = hls_attr_read(value, nv, attrs[i].cls, attrs[i].ptr);
				break;
			}
		}
	}

	return r;
}

static int hls_ext_inf(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	size_t n;
	const char* duration;
	struct hls_segment_t* segment;
	segment = parser->segment;

	n = hls_strsplit(data, data + bytes, ",", "\"", (const char**)&segment->title);
	duration = hls_strtrim(data, &n, " \t", " \t"); // trim SP/HTAB

	// decimal-floating-point or decimal-integer number
	segment->duration = strtod(duration, NULL);
	n = data + bytes - segment->title;
	segment->title = (char*)hls_strtrim(segment->title, &n, " \t'\"", " \t'\""); // trim SP/HTAB/'/"
	segment->title[n] = 0;
	return 0;
}

static int hls_ext_x_byterange(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	size_t n;
	const char* ptr, *next;
	struct hls_segment_t* segment;
	segment = parser->segment;

	n = hls_strsplit(data, data + bytes, "@", "", &next);
	ptr = hls_strtrim(data, &n, " \t", " \t"); // trim SP/HTAB
	segment->bytes = (int64_t)strtoull(ptr, NULL, 10); // decimal-integer

	n = data + bytes - next;
	next = hls_strtrim(next, &n, " \t'\"", " \t'\""); // trim SP/HTAB/'/"
	segment->offset = n > 0 ? (int64_t)strtoull(next, NULL, 10) : segment->offset;
	return 0;
}

static int hls_ext_x_discontinuity(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	parser->segment->discontinuity = 1;
	(void)data, (void)bytes;
	return 0;
}

static int hls_ext_x_key(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	int r;
	struct hls_segment_t* segment;
	struct hls_tag_attr_t attrs[5];
	char* iv;

	iv = "";
	segment = parser->segment;
	HLS_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "METHOD", &segment->key.method);
	HLS_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "URI", &segment->key.uri);
	HLS_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_STRING, "IV", &iv);
	HLS_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "KEYFORMAT", &segment->key.keyformat);
	HLS_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_STRING, "KEYFORMATVERSIONS", &segment->key.keyformatversions);

	r = hls_parse_attrs(data, bytes, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	if (strlen(iv) == 34) // 0x + 32bits
		hls_base16_decode(segment->key.iv, iv + 2, 32);

	// It applies to every Media Segment and to every Media
	// Initialization Section declared by an EXT-X-MAP tag that appears
	// between it and the next EXT-X-KEY tag in the Playlist file with the
	// same KEYFORMAT attribute (or the end of the Playlist file).
	parser->key_segment = parser->segment; // save key_segment
	return 0;
}

static int hls_ext_x_map(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	int r;
	size_t n;
	struct hls_segment_t* segment;
	struct hls_tag_attr_t attrs[2];
	char* byterange;
	const char* ptr, *next;

	byterange = "";
	segment = parser->segment;
	HLS_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "URI", &segment->map.uri);
	HLS_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "BYTERANGE", &byterange);
	
	r = hls_parse_attrs(data, bytes, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	n = strlen(byterange);
	if (n > 2)
	{
		n = hls_strsplit(byterange, byterange + n, "@", "", &next);
		ptr = hls_strtrim(byterange, &n, " \t", " \t"); // trim SP/HTAB
		segment->map.bytes = (int64_t)strtoull(ptr, NULL, 10); // decimal-integer

		n = byterange + n - next;
		next = hls_strtrim(next, &n, " \t'\"", " \t'\""); // trim SP/HTAB/'/"
		segment->map.offset = n > 0 ? (int64_t)strtoull(next, NULL, 10) : segment->map.offset;
	}
	return 0;
}

static int hls_ext_x_program_date_time(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	struct hls_segment_t* segment;
	segment = parser->segment;
	segment->program_date_time = (char*)hls_strtrim(data, &bytes, " \t'\"", " \t'\""); // trim SP/HTAB/'/"
	return 0;
}

static int hls_ext_x_daterange(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	struct hls_segment_t* segment;
	struct hls_tag_attr_t attrs[7];
	
	segment = parser->segment;
	HLS_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "ID", &segment->daterange.id);
	HLS_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "CLASS", &segment->daterange.cls);
	HLS_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_STRING, "START-DATE", &segment->daterange.start_date);
	HLS_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "END-DATE", &segment->daterange.end_date);
	HLS_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_FLOAT64, "DURATION", &segment->daterange.duration);
	HLS_TAG_ATTR_VALUE(attrs[5], ATTR_VALUE_TYPE_FLOAT64, "PLANNED-DURATION", &segment->daterange.planned_duration);
	HLS_TAG_ATTR_VALUE(attrs[6], ATTR_VALUE_TYPE_STRING_BOOL, "END-ON-NEXT", &segment->daterange.end_on_next);

	return hls_parse_attrs(data, bytes, attrs, sizeof(attrs) / sizeof(attrs[0]));
}

static int hls_ext_x_targetduration(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	data = hls_strtrim(data, &bytes, " \t'\"", " \t'\""); // trim SP/HTAB/'/"
	parser->playlist->target_duration = (uint64_t)strtoull(data, NULL, 10); // decimal-integer
	return 0;
}

static int hls_ext_x_media_sequence(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	data = hls_strtrim(data, &bytes, " \t'\"", " \t'\""); // trim SP/HTAB/'/"
	parser->playlist->media_sequence = (uint64_t)strtoull(data, NULL, 10); // decimal-integer
	return 0;
}

static int hls_ext_x_discontinuity_sequence(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	data = hls_strtrim(data, &bytes, " \t'\"", " \t'\""); // trim SP/HTAB/'/"
	parser->playlist->discontinuity_sequence = (uint64_t)strtoull(data, NULL, 10); // decimal-integer
	return 0;
}

static int hls_ext_x_endlist(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	data = hls_strtrim(data, &bytes, " \t'\"", " \t'\""); // trim SP/HTAB/'/"
	parser->playlist->endlist = 1;
	return 0;
}

static int hls_ext_x_playlist_type(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	struct hls_playlist_t* playlist;
	playlist = parser->playlist;

	data = hls_strtrim(data, &bytes, " \t'\"", " \t'\""); // trim SP/HTAB/'/"
	if (5 == bytes && 0 == strncasecmp("EVENT", data, 5))
		playlist->type = HLS_PLAYLIST_TYPE_EVENT;
	else if (3 == bytes && 0 == strncasecmp("VOD", data, 3))
		playlist->type = HLS_PLAYLIST_TYPE_VOD;
	else
		playlist->type = HLS_PLAYLIST_TYPE_LIVE;
	return 0;
}

static int hls_ext_x_i_frames_only(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	data = hls_strtrim(data, &bytes, " \t'\"", " \t'\""); // trim SP/HTAB/'/"
	parser->playlist->i_frames_only = 1;
	return 0;
}

// If the media type is VIDEO or AUDIO, a missing URI attribute
// indicates that the media data for this Rendition is included in the
// Media Playlist of any EXT-X-STREAM-INF tag referencing this EXT-X-MEDIA tag.
static int hls_ext_x_media(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	struct hls_media_t* media;
	struct hls_master_t* master;
	struct hls_tag_attr_t attrs[12];

	master = parser->master;
	if (0 != hls_parser_realloc((void**)&master->medias, &parser->media_capacity, master->media_count, 4, sizeof(struct hls_media_t)))
		return -ENOMEM;

	media = &master->medias[master->media_count++];
	HLS_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "TYPE", &media->type);
	HLS_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "URI", &media->uri);
	HLS_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_STRING, "GROUP-ID", &media->group_id);
	HLS_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "LANGUAGE", &media->language);
	HLS_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_STRING, "ASSOC-LANGUAGE", &media->assoc_language);
	HLS_TAG_ATTR_VALUE(attrs[5], ATTR_VALUE_TYPE_STRING, "NAME", &media->name);
	HLS_TAG_ATTR_VALUE(attrs[6], ATTR_VALUE_TYPE_STRING_BOOL, "DEFAULT", &media->is_default);
	HLS_TAG_ATTR_VALUE(attrs[7], ATTR_VALUE_TYPE_STRING_BOOL, "AUTOSELECT", &media->autoselect);
	HLS_TAG_ATTR_VALUE(attrs[8], ATTR_VALUE_TYPE_STRING_BOOL, "FORCED", &media->forced);
	HLS_TAG_ATTR_VALUE(attrs[9], ATTR_VALUE_TYPE_STRING, "INSTREAM-ID", &media->instream_id);
	HLS_TAG_ATTR_VALUE(attrs[10], ATTR_VALUE_TYPE_STRING, "CHARACTERISTICS", &media->characteristics);
	HLS_TAG_ATTR_VALUE(attrs[11], ATTR_VALUE_TYPE_STRING, "CHANNELS", &media->channels);

	return hls_parse_attrs(data, bytes, attrs, sizeof(attrs) / sizeof(attrs[0]));
}

static int hls_ext_x_stream_inf(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	int r;
	struct hls_variant_t* variant;
	struct hls_tag_attr_t attrs[10];
	char* resolution;
	
	resolution = "";
	variant = parser->variant;
	HLS_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_UINT32, "BANDWIDTH", &variant->bandwidth);
	HLS_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_UINT32, "AVERAGE-BANDWIDTH", &variant->average_bandwidth);
	HLS_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_STRING, "CODECS", &variant->codecs);
	HLS_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "RESOLUTION", &resolution);
	HLS_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_FLOAT64, "FRAME-RATE", &variant->fps);
	HLS_TAG_ATTR_VALUE(attrs[5], ATTR_VALUE_TYPE_STRING, "HDCP-LEVEL", &variant->hdcp_level);
	HLS_TAG_ATTR_VALUE(attrs[6], ATTR_VALUE_TYPE_STRING, "AUDIO", &variant->audio);
	HLS_TAG_ATTR_VALUE(attrs[7], ATTR_VALUE_TYPE_STRING, "VIDEO", &variant->video);
	HLS_TAG_ATTR_VALUE(attrs[8], ATTR_VALUE_TYPE_STRING, "SUBTITLES", &variant->subtitle);
	HLS_TAG_ATTR_VALUE(attrs[9], ATTR_VALUE_TYPE_STRING, "CLOSED-CAPTIONS", &variant->closed_captions);

	r = hls_parse_attrs(data, bytes, attrs, sizeof(attrs) / sizeof(attrs[0])); 
	if (0 != r)
		return r;

	if (2 != sscanf(resolution, "%dx%d", &variant->width, &variant->height))
		return 0; // ignore
	return 0;
}

static int hls_ext_x_i_frame_stream_inf(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	int r;
	struct hls_variant_t* variant;
	struct hls_variant_t* iframestream;
	struct hls_tag_attr_t attrs[7];
	char* resolution;

	resolution = "";
	variant = parser->variant;
	if (!variant->i_frame_stream_inf)
	{
		variant->i_frame_stream_inf = calloc(1, sizeof(*variant->i_frame_stream_inf));
		if (!variant->i_frame_stream_inf)
			return -ENOMEM;
	}
	iframestream = variant->i_frame_stream_inf;

	HLS_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "URI", &iframestream->uri);
	HLS_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_UINT32, "BANDWIDTH", &iframestream->bandwidth);
	HLS_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_UINT32, "AVERAGE-BANDWIDTH", &iframestream->average_bandwidth);
	HLS_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "CODECS", &iframestream->codecs);
	HLS_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_STRING, "RESOLUTION", &resolution);
	HLS_TAG_ATTR_VALUE(attrs[5], ATTR_VALUE_TYPE_STRING, "HDCP-LEVEL", &iframestream->hdcp_level);
	HLS_TAG_ATTR_VALUE(attrs[6], ATTR_VALUE_TYPE_STRING, "VIDEO", &iframestream->video);
	
	r = hls_parse_attrs(data, bytes, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	if (2 != sscanf(resolution, "%dx%d", &iframestream->width, &iframestream->height))
		return 0; // ignore
	return 0;
}

static int hls_ext_x_session_data(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	struct hls_master_t* master;
	struct hls_tag_attr_t attrs[4];

	master = parser->master;
	if (0 != hls_parser_realloc((void**)&master->session_data, &parser->session_data_capacity, master->session_data_count, 2, sizeof(master->session_data[0])))
		return -ENOMEM;

	HLS_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "DATA-ID", &master->session_data[master->session_data_count].data_id);
	HLS_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "VALUE", &master->session_data[master->session_data_count].value);
	HLS_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_STRING, "URI", &master->session_data[master->session_data_count].uri);
	HLS_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "LANGUAGE", &master->session_data[master->session_data_count].language);
	
	++master->session_data_count;
	return hls_parse_attrs(data, bytes, attrs, sizeof(attrs) / sizeof(attrs[0]));
}

static int hls_ext_x_session_key(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	int r;
	struct hls_master_t* master;
	struct hls_tag_attr_t attrs[5];
	char* iv;

	iv = "";
	master = parser->master;
	if (0 != hls_parser_realloc((void**)&master->session_key, &parser->session_key_capacity, master->session_key_count, 2, sizeof(master->session_key[0])))
		return -ENOMEM;

	HLS_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "METHOD", &master->session_key[master->session_key_count].method);
	HLS_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "URI", &master->session_key[master->session_key_count].uri);
	HLS_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_STRING, "IV", &iv);
	HLS_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "KEYFORMAT", &master->session_key[master->session_key_count].keyformat);
	HLS_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_STRING, "KEYFORMATVERSIONS", &master->session_key[master->session_key_count].keyformatversions);

	r = hls_parse_attrs(data, bytes, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	if (strlen(iv) == 34) // 0x + 32bits
		hls_base16_decode(master->session_key[master->session_key_count].iv, iv + 2, 32);

	++master->session_key_count;
	return 0;
}

static int hls_ext_x_independent_segments(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	data = hls_strtrim(data, &bytes, " \t'\"", " \t'\""); // trim SP/HTAB/'/"
	if (parser->playlist)
		parser->playlist->independent_segments = 1;
	else
		parser->master->independent_segments = 1;
	return 0;
}

static int hls_ext_x_start(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	size_t n;
	int start_precise;
	double start_time_offset;
	const char* ptr, * next;

	n = hls_strsplit(data, data + bytes, ",", "", &next);
	ptr = hls_strtrim(data, &n, " \t", " \t"); // trim SP/HTAB
	start_time_offset = strtod(ptr, NULL); // signed-decimal-floating-point

	n = data + bytes - next;
	next = hls_strtrim(next, &n, " \t'\"", " \t'\""); // trim SP/HTAB/'/"
	start_precise = 3 == n && 0 == strncasecmp("YES", next, 3) ? 1 : 0;

	if (parser->playlist)
	{
		parser->playlist->start_time_offset = start_time_offset;
		parser->playlist->start_precise = start_precise;
	}
	else
	{
		parser->master->start_time_offset = start_time_offset;
		parser->master->start_precise = start_precise;
	}
	return 0;
}

static int hls_ext_x_version(struct hls_parser_t* parser, const char* data, size_t bytes)
{
	int version;
	data = hls_strtrim(data, &bytes, " \t", " \t"); // trim SP/HTAB
	version = (int)strtoul(data, NULL, 10);
	if (parser->playlist)
		parser->playlist->version = version;
	else
		parser->master->version = version;
	return 0;
}

static int hls_parser_onuri(struct hls_parser_t* parser, const char* uri, size_t len)
{
	//printf("uri: %.*s\n", (int)len, uri);

	uri = hls_strtrim(uri, &len, " \t", " \t"); // trim SP/HTAB

	if (parser->playlist)
	{
		if (!parser->segment)
		{
			assert(0);
			return -1;
		}
		parser->segment->uri = (char*)uri;
		parser->segment->uri[len] = 0;
		parser->segment = NULL;
		parser->playlist->count++;
	}
	else
	{
		if (!parser->variant)
		{
			assert(0);
			return -1;
		}
		parser->variant->uri = (char*)uri;
		parser->variant->uri[len] = 0;
		parser->variant = NULL;
		parser->master->variant_count++;
	}

	return 0;
}

static const struct
{
	int cls; // 0-any, 1-playlist, 2-master
	const char* name;
	int (*parser)(struct hls_parser_t* parser, const char* attrs, size_t bytes);
} s_tags[] = {
	// 4.3.2.  Media Segment Tags
	{ HLS_M3U8_PLAYLIST, "EXTINF",						hls_ext_inf },
	{ HLS_M3U8_PLAYLIST, "EXT-X-BYTERANGE",				hls_ext_x_byterange },
	{ HLS_M3U8_PLAYLIST, "EXT-X-DISCONTINUITY",			hls_ext_x_discontinuity },
	{ HLS_M3U8_PLAYLIST, "EXT-X-KEY",					hls_ext_x_key },
	{ HLS_M3U8_PLAYLIST, "EXT-X-MAP",					hls_ext_x_map },
	{ HLS_M3U8_PLAYLIST, "EXT-X-PROGRAM-DATE-TIME",		hls_ext_x_program_date_time },
	{ HLS_M3U8_PLAYLIST, "EXT-X-DATERANGE",				hls_ext_x_daterange },
	
	// 4.3.3.  Media Playlist Tags
	{ HLS_M3U8_PLAYLIST, "EXT-X-TARGETDURATION",		hls_ext_x_targetduration },
	{ HLS_M3U8_PLAYLIST, "EXT-X-MEDIA-SEQUENCE",		hls_ext_x_media_sequence },
	{ HLS_M3U8_PLAYLIST, "EXT-X-DISCONTINUITY-SEQUENCE",hls_ext_x_discontinuity_sequence },
	{ HLS_M3U8_PLAYLIST, "EXT-X-ENDLIST",				hls_ext_x_endlist },
	{ HLS_M3U8_PLAYLIST, "EXT-X-PLAYLIST-TYPE",			hls_ext_x_playlist_type },
	{ HLS_M3U8_PLAYLIST, "EXT-X-I-FRAMES-ONLY",			hls_ext_x_i_frames_only },

	// 4.3.4.  Master Playlist Tags
	{ HLS_M3U8_MASTER, "EXT-X-MEDIA",					hls_ext_x_media },
	{ HLS_M3U8_MASTER, "EXT-X-STREAM-INF",				hls_ext_x_stream_inf },
	{ HLS_M3U8_MASTER, "EXT-X-I-FRAME-STREAM-INF",		hls_ext_x_i_frame_stream_inf },
	{ HLS_M3U8_MASTER, "EXT-X-SESSION-DATA",			hls_ext_x_session_data },
	{ HLS_M3U8_MASTER, "EXT-X-SESSION-KEY",				hls_ext_x_session_key },
	
	// 4.3.5.  Media or Master Playlist Tags
	{ HLS_M3U8_UNKNOWN, "EXT-X-INDEPENDENT-SEGMENTS",	hls_ext_x_independent_segments },
	{ HLS_M3U8_UNKNOWN, "EXT-X-START",					hls_ext_x_start },

	{ HLS_M3U8_UNKNOWN, "EXT-X-VERSION",				hls_ext_x_version},
};

static int hls_parser_realloc(void** ptr, size_t* capacity, size_t len, size_t incr, size_t size)
{
	size_t n;
	void* ptr1;
	if (len >= *capacity)
	{
		n = len / 4;
		n = n > incr ? n : incr;
		ptr1 = realloc(*ptr, (len + 1 + n) * size);
		if (!ptr1)
			return -ENOMEM;

		memset((uint8_t*)ptr1 + len * size, 0, (1 + n) * size);
		*capacity = len + 1 + n;
		*ptr = ptr1;
	}
	return 0;
}

static int hls_parser_fetch_segment(struct hls_parser_t* parser)
{
	if (parser->segment)
		return 0;
	
	if(0 != hls_parser_realloc((void**)&parser->playlist->segments, &parser->segment_capacity, parser->playlist->count, 8, sizeof(struct hls_segment_t)))
		return -ENOMEM;

	parser->segment = &parser->playlist->segments[parser->playlist->count];
	//memset(parser->segment, 0, sizeof(*parser->segment));
	parser->segment->bytes = -1;
	parser->segment->offset = -1;
	parser->segment->map.bytes = -1;
	parser->segment->map.offset = -1;

	// copy default key
	if (parser->key_segment)
		memcpy(&parser->segment->key, &parser->key_segment->key, sizeof(parser->segment->key));
	return 0;
}

static int hls_parser_fetch_variant(struct hls_parser_t* parser)
{
	if (parser->variant)
		return 0;

	if (0 != hls_parser_realloc((void**)&parser->master->variants, &parser->variant_capacity, parser->master->variant_count, 4, sizeof(struct hls_variant_t)))
		return -ENOMEM;

	parser->variant = &parser->master->variants[parser->master->variant_count];
	//memset(parser->variant, 0, sizeof(*parser->variant));
	return 0;
}

static int hls_parser_ontag(struct hls_parser_t* parser, const char* tag, size_t len, const char* attrs, size_t bytes)
{
	int r;
	size_t i;

	//printf("%.*s: %.*s\n", (int)len, tag, (int)bytes, attrs);
	r = parser->playlist ? hls_parser_fetch_segment(parser) : hls_parser_fetch_variant(parser);
	if (0 != r)
		return r;

	for (i = 0; i < sizeof(s_tags) / sizeof(s_tags[0]); i++)
	{
		if (len == strlen(s_tags[i].name)+1 && 0 == strncasecmp(s_tags[i].name, tag+1, len-1))
		{
			if ((HLS_M3U8_PLAYLIST == s_tags[i].cls && !parser->playlist)
				|| (HLS_M3U8_MASTER == s_tags[i].cls && !parser->master))
				return -EINVAL;

			r = s_tags[i].parser(parser, attrs, bytes);
			break;
		}
	}

	return r;
}

static int hls_parser_input(struct hls_parser_t* parser, const char* m3u8, size_t len)
{
	int r;
	size_t n, ntag;
	const char* ptr, *next;
	const char* attr;

	r = 0;
	for(ptr = m3u8; ptr && ptr < m3u8 + len; ptr = next)
	{
		n = hls_strsplit(ptr, m3u8 + len, "\r\n", "", &next);
		ptr = hls_strtrim(ptr, &n, " \t", " \t"); // trim SP/HTAB
		if (n < 1)
			continue; // blank line

		if ('#' == *ptr)
		{
			if (n <= 4 || strncmp("#EXT", ptr, 4))
			{
				// ignore comment
				//assert(0);
				continue;
			}
			else
			{
				// tags
				attr = strpbrk(ptr, ": \t\r\n");
				assert(attr <= ptr + n);
				ntag = attr ? attr - ptr : n;
				attr = attr ? attr + strspn(attr, ": \t") : ptr + n;
				r = hls_parser_ontag(parser, ptr, ntag, attr, ptr + n - attr);
			}
		}
		else
		{
			// uri
			r = hls_parser_onuri(parser, ptr, n);
		}
	}

	return r;
}

int hls_master_parse(struct hls_master_t** master, const char* m3u8, size_t len)
{
	int r;
	char* ptr;
	struct hls_parser_t parser;

	memset(&parser, 0, sizeof(parser));
	parser.master = (struct hls_master_t*)calloc(1, sizeof(struct hls_master_t) + len + 1);
	if (!parser.master)
		return -ENOMEM;

	ptr = (char*)(parser.master + 1);
	memcpy(ptr, m3u8, len);

	r = hls_parser_input(&parser, ptr, len);
	if (0 != r)
	{
		hls_master_free(&parser.master);
		return r;
	}

	*master = parser.master;
	return 0;
}

int hls_playlist_parse(struct hls_playlist_t** playlist, const char* m3u8, size_t len)
{
	int r;
	char* ptr;
	struct hls_parser_t parser;

	memset(&parser, 0, sizeof(parser));
	parser.playlist = (struct hls_playlist_t*)calloc(1, sizeof(struct hls_playlist_t) + len + 1);
	if (!parser.playlist)
		return -ENOMEM;

	ptr = (char*)(parser.playlist + 1);
	memcpy(ptr, m3u8, len);

	r = hls_parser_input(&parser, ptr, len);
	if (0 != r)
	{
		hls_playlist_free(&parser.playlist);
		return r;
	}

	*playlist = parser.playlist;
	return 0;
}

int hls_master_free(struct hls_master_t** master)
{
	size_t i;
	struct hls_master_t* p;
	if (master && *master)
	{
		p = *master;
		if (p->medias)
			free(p->medias);
		if (p->session_data)
			free(p->session_data);
		if (p->session_key)
			free(p->session_key);
		if (p->variants)
		{
			for (i = 0; i < p->variant_count; i++)
			{
				if (p->variants[i].i_frame_stream_inf)
					free(p->variants[i].i_frame_stream_inf);
			}
			free(p->variants);
		}
		free(p);
		*master = NULL;
		return 0;
	}
	return -1;
}

int hls_playlist_free(struct hls_playlist_t** playlist)
{
	struct hls_playlist_t* p;
	if (playlist && *playlist)
	{
		p = *playlist;
		if (p->segments)
			free(p->segments);
		free(p);
		*playlist = NULL;
		return 0;
	}
	return -1;
}

int hls_parser_probe(const char* m3u8, size_t len)
{
	size_t n;
	const char* ptr, *next;

	for (ptr = m3u8; ptr && ptr < m3u8 + len; ptr = next)
	{
		n = hls_strsplit(ptr, m3u8 + len, "\r\n", "", &next);
		ptr = hls_strtrim(ptr, &n, " \t", " \t"); // trim SP/HTAB
		if (n >= 7 && 0 == strncasecmp("#EXTINF", ptr, 7))
			return HLS_M3U8_PLAYLIST;
		else if (n >= 17 && 0 == strncasecmp("#EXT-X-STREAM-INF", ptr, 17))
			return HLS_M3U8_MASTER;
	}

	return HLS_M3U8_UNKNOWN;
}

#if defined(_DEBUG) || defined(DEBUG)
void hls_parser_test(const char* m3u8)
{
	static char data[2 * 1024 * 1024];
	FILE* fp = fopen(m3u8, "rb");
	int n = (int)fread(data, 1, sizeof(data), fp);
	fclose(fp);

	int v = hls_parser_probe(data, n);
	if (HLS_M3U8_MASTER == v)
	{
		struct hls_master_t* master;
		assert(0 == hls_master_parse(&master, data, n));
		hls_master_free(&master);
	}
	else if (HLS_M3U8_PLAYLIST == v)
	{
		struct hls_playlist_t* playlist;
		assert(0 == hls_playlist_parse(&playlist, data, n));
		hls_playlist_free(&playlist);
	}
	else
	{
		assert(0);
	}
}
#endif
