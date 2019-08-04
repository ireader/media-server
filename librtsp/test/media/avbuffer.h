#ifndef _avbuffer_h_
#define _avbuffer_h_

#include <stdint.h>

struct avbuffer_t
{
	volatile int32_t refcount;

	uint8_t *data;
	int		size; // capacity

	void (*free)(void *opaque, void* data);
	void *opaque;
};

#ifdef __cplusplus
extern "C" {
#endif

///@param[in] size alloc packet data size, don't include sizeof(struct avpacket_t)
///@return alloc new avpacket_t, use avpacket_release to free memory
struct avbuffer_t* avbuffer_alloc(int size);
int32_t avbuffer_addref(struct avbuffer_t* buf);
int32_t avbuffer_release(struct avbuffer_t* buf);

#ifdef __cplusplus
}
#endif
#endif /* !_avbuffer_h_ */
