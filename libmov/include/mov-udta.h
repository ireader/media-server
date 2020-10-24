#ifndef _mov_udta_h_
#define _mov_udta_h_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


// https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/Metadata/Metadata.html

struct mov_udta_meta_t
{
	//char* title;
	//char* artist;
	//char* album_artist;
	//char* album;
	//char* date;
	//char* comment;
	//char* genre;
	//char* copyright;
	//char* lyrics;
	//char* description;
	//char* synopsis;
	//char* show;
	//char* episode_id;
	//char* network;
	//char* keywords;
	//char* season_num;
	//char* media_type;
	//char* hd_video;
	//char* gapless_playback;
	//char* compilation;

	uint8_t* cover; // cover binary data, jpeg/png only
	int cover_size; // cover binnary data length in byte
};

int mov_udta_meta_write(const struct mov_udta_meta_t* meta, void* data, int bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_mov_udta_h_ */
