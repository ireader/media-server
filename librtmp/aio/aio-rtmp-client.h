#ifndef _aio_rtmp_client_h_
#define _aio_rtmp_client_h_

#include "aio-socket.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct aio_rtmp_client_t aio_rtmp_client_t;

struct aio_rtmp_client_handler_t
{
	/// aio transport close
	void (*onclose)(void* param);

	// play only
	int (*onvideo)(void* param, const void* video, size_t bytes, uint32_t timestamp);
	int (*onaudio)(void* param, const void* audio, size_t bytes, uint32_t timestamp);

	// publish only
	void (*onready)(void* param);
	void (*onsend)(void* param, int code, size_t bytes);
};

aio_rtmp_client_t* aio_rtmp_client_create(aio_socket_t aio, const char* app, const char* stream, const char* tcurl, struct aio_rtmp_client_handler_t* handler, void* param);
void aio_rtmp_client_destroy(aio_rtmp_client_t* client);

///@param[in] publish, see @rtmp_client_start
///@param[in] publish, 0-Publish(push stream to server), 1-LIVE/VOD(pull from server), 2-LIVE only, 3-VOD only
///@return 0-ok, other-error
int aio_rtmp_client_start(aio_rtmp_client_t* client, int publish);
int aio_rtmp_client_stop(aio_rtmp_client_t* client);

int aio_rtmp_client_send_audio(aio_rtmp_client_t* client, const void* flv, size_t bytes, uint32_t timestamp);
int aio_rtmp_client_send_video(aio_rtmp_client_t* client, const void* flv, size_t bytes, uint32_t timestamp);
size_t aio_rtmp_client_get_unsend(aio_rtmp_client_t* client);

#ifdef __cplusplus
}
#endif
#endif /* !_aio_rtmp_client_h_ */
