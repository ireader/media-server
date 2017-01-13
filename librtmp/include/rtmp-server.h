#ifndef _rtmp_server_h_
#define _rtmp_server_h_

#ifdef __cplusplus
extern "C" {
#endif

#include "rtmp-message.h"

struct rtmp_server_handler_t
{
	void (*on_connect)(void* param, struct rtmp_trunk_header_t* header, const uint8_t* payload);
	void (*on_create_stream)(void* param, struct rtmp_trunk_header_t* header, const uint8_t* payload);
	void (*on_delete_stream)(void* param, struct rtmp_trunk_header_t* header, const uint8_t* payload);
	
	void (*on_play)(void* param, struct rtmp_trunk_header_t* header, const uint8_t* payload);
	void (*on_pause)(void* param, struct rtmp_trunk_header_t* header, const uint8_t* payload);
	void (*on_seek)(void* param, struct rtmp_trunk_header_t* header, const uint8_t* payload);

	void (*on_recv_audio)(void* param, struct rtmp_trunk_header_t* header, const uint8_t* payload);
	void (*on_recv_video)(void* param, struct rtmp_trunk_header_t* header, const uint8_t* payload);

	void (*on_publish)(void* param, struct rtmp_trunk_header_t* header, const uint8_t* payload);
	void (*on_fcpublish)(void* param, struct rtmp_trunk_header_t* header, const uint8_t* payload);
	void (*on_fcunpublish)(void* param, struct rtmp_trunk_header_t* header, const uint8_t* payload);
};

void* rtmp_server_create(void* param, const struct rtmp_server_handler_t* handler);

void rtmp_server_destroy(void** rtmp);

int rtmp_server_state(void* rtmp);

/// @param[in] rtmp rtmp_server_create instance
/// @param[in] data rtmp chunk stream data
/// @param[in] bytes data length
/// @return 0-ok, other-error
int rtmp_server_input(void* rtmp, const uint8_t* data, size_t bytes);

/// send audio/video data(VOD only)
/// @param[in] rtmp rtmp_server_create instance
int rtmp_server_send_audio(void* rtmp);
int rtmp_server_send_video(void* rtmp);
int rtmp_server_send_metadata(void* rtmp);

#ifdef __cplusplus
}
#endif
#endif /* !_rtmp_server_h_ */
