#ifndef _hls_parser_h_
#define _hls_parser_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
	
enum
{
	HLS_M3U8_UNKNOWN = 0,
	HLS_M3U8_PLAYLIST,
	HLS_M3U8_MASTER,
};

enum
{
	HLS_PLAYLIST_TYPE_LIVE = 0,
	HLS_PLAYLIST_TYPE_EVENT,
	HLS_PLAYLIST_TYPE_VOD,
};

enum
{
	HLS_MEDIA_UNKNOWN = 0,
	HLS_MEDIA_AUDIO,
	HLS_MEDIA_VIDEO,
	HLS_MEDIA_SUBTITLE,
	HLS_MEDIA_CLOSED_CAPTIONS,
};

// #EXT-X-MEDIA
struct hls_media_t
{
	char* type; // audio/video/subtitle/closed-captions
	char* uri; // Media Playlist file, If the TYPE is CLOSED-CAPTIONS, the URI attribute MUST NOT be present.

	char* group_id;
	char* language; // RFC5645
	char* assoc_language;
	char* name;
	char* instream_id; // CC1, SERVICE1, for HLS_MEDIA_CLOSED_CAPTIONS
	char* characteristics; // public.accessibility.describes-video
	char* channels; // for HLS_MEDIA_AUDIO

	int autoselect; // 1-YES, 0-NO
	int is_default; // 1-YES, 0-NO
	int forced; // 1-YES, 0-NO, SUBTITLES only
};

// #EXT-X-STREAM-INF
struct hls_variant_t
{
	uint32_t bandwidth; // the peak segment bit rate of the Variant Stream
	uint32_t average_bandwidth; // the average segment bit rate of the Variant Stream
	int width, height; // resolution
	double fps; // frame-rate
	char* hdcp_level; // TYPE-0
	char* uri;
	char* codecs; // RFC6381, mp4a.40.2,avc1.4d401e
	char* video_range; // SDR/PQ
	char* audio; // GROUP-ID of EXT-X-MEDIA
	char* video; // GROUP-ID of EXT-X-MEDIA
	char* subtitle; // GROUP-ID of EXT-X-MEDIA
	char* closed_captions; // GROUP-ID of EXT-X-MEDIA

	struct hls_variant_t* i_frame_stream_inf; // EXT-X-I-FRAME-STREAM-INF
};

struct hls_segment_t
{
	double duration;
	char* uri;
	char* title;

	int64_t bytes; // EXT-X-BYTERANGE, -1 if don't exist
	int64_t offset; // EXT-X-BYTERANGE, -1 if don't exist

	// 1. file format
	// 2. number, type, and identifiers of tracks
	// 3. timestamp sequence
	// 4. encoding parameters
	// 5. encoding sequence
	int discontinuity; // EXT-X-DISCONTINUITY

	struct {
		char* method; // NONE, AES-128, and SAMPLE-AES
		char* uri;
		char* keyformat; // identity
		char* keyformatversions; // "1", "1/2", "1/2/5"
		uint8_t iv[16]; // 128bits
	} key; // EXT-X-KEY

	// It applies to every Media Segment that appears after it in the
	// Playlist until the next EXT-X-MAP tag or until the end of the Playlist.
	struct {
		char* uri;
		int64_t bytes; // BYTERANGE, -1 if don't exist
		int64_t offset; // BYTERANGE, -1 if don't exist
	} map; // EXT-X-MAP

	char* program_date_time; // EXT-X-PROGRAM-DATE-TIME

	struct {
		char* id;
		char* cls;
		char* start_date;
		char* end_date;
		double duration; // decimal-floating-point number of seconds
		double planned_duration; // decimal-floating-point number of seconds
		char* x_client_attribute; // TODO
		int end_on_next; // 1-YES, 0-NO
	} daterange; // EXT-X-DATERANGE
};

// If the Media Playlist contains the EXT-X-MEDIA-SEQUENCE tag, the
// client SHOULD assume that each Media Segment in it will become
// unavailable at the time that the Playlist file was loaded plus the
// duration of the Playlist file.
struct hls_playlist_t
{
	int version;

	uint64_t target_duration; // EXT-X-TARGETDURATION, seconds
	uint64_t media_sequence; // EXT-X-MEDIA-SEQUENCE, base from 0
	uint64_t discontinuity_sequence; // EXT-X-DISCONTINUITY-SEQUENCE, decimal-integer
	int endlist; // EXT-X-ENDLIST, 1-endlist, 0-don't have endlist
	int type; // EXT-X-PLAYLIST-TYPE, 0-LIVE, 1-EVENT, 2-VOD
	int i_frames_only; // EXT-X-I-FRAMES-ONLY
	int independent_segments; // EXT-X-INDEPENDENT-SEGMENTS
	double start_time_offset; // EXT-X-START, seconds
	int start_precise; // EXT-X-START

	struct hls_segment_t* segments;
	size_t count;
};

struct hls_master_t
{
	int version;

	size_t variant_count;
	struct hls_variant_t *variants;

	// renditions: audio, video, subtitle, close_captions
	size_t media_count;
	struct hls_media_t* medias;
	
	struct {
		char* data_id;
		char* value;
		char* uri;
		char* language;
	} *session_data;
	size_t session_data_count;

	struct {
		char* method; // NONE, AES-128, and SAMPLE-AES
		char* uri;
		char* keyformat; // identity
		char* keyformatversions; // "1", "1/2", "1/2/5"
		uint8_t iv[16]; // 128bits
	} *session_key; // EXT-X-SESSION-KEY
	size_t session_key_count;

	int independent_segments; // EXT-X-INDEPENDENT-SEGMENTS
	double start_time_offset; // EXT-X-START
	int start_precise; // EXT-X-START
};

/// Probe m3u8 content type
/// @return HLS_M3U8_PLAYLIST-media playlist, HLS_M3U8_MASTER-master playlist, other-unknown
int hls_parser_probe(const char* m3u8, size_t len);

/// Parse m3u8 master playlist
/// @param[out] master m3u8 master playlist(free by hls_master_free)
/// @return 0-ok, other-error
int hls_master_parse(struct hls_master_t** master, const char* m3u8, size_t len);

int hls_master_free(struct hls_master_t** master);

int hls_master_best_variant(const struct hls_master_t* master);

int hls_master_rendition(const struct hls_master_t* master, int variant, int type, const char* name);

/// Parse m3u8 media playlist
/// @param[out] playlist m3u8 media playlist(free by hls_playlist_free)
/// @return 0-ok, other-error
int hls_playlist_parse(struct hls_playlist_t** playlist, const char* m3u8, size_t len);

int hls_playlist_free(struct hls_playlist_t** playlist);

/// @return total duration in MS
int64_t hls_playlist_duration(const struct hls_playlist_t* playlist);

#ifdef __cplusplus
}
#endif
#endif /* !_hls_parser_h_ */
