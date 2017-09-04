#ifndef _dash_mpd_h_
#define _dash_mpd_h_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dash_mpd_t dash_mpd_t;

struct dash_mpd_notify_t
{
	int (*onupdate)(void* param);
};

dash_mpd_t* dash_mpd_create(const char* name, int flags, struct dash_mpd_notify_t* notify, void* param);
void dash_mpd_destroy(dash_mpd_t* mpd);

int dash_mpd_add_video_adapation_set(dash_mpd_t* mpd, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size);
int dash_mpd_add_audio_adapation_set(dash_mpd_t* mpd, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size);

int dash_mpd_input(dash_mpd_t* mpd, int adapation, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags);

size_t dash_mpd_playlist(dash_mpd_t* mpd, char* playlist, size_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_dash_mpd_h_ */
