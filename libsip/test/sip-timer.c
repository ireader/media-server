#include "sip-timer.h"
#include "aio-timeout.h"
#include <stdlib.h>

void* sip_timer_start(int timeout, sip_timer_handle handler, void* usrptr)
{
	struct aio_timeout_t* t;
	t = calloc(1, sizeof(struct aio_timeout_t));
	if (0 == aio_timeout_start(t, timeout, handler, usrptr))
		return t;
	free(t);
	return NULL;
}

int sip_timer_stop(void* id)
{
	int r;
	struct aio_timeout_t* t;
	t = (struct aio_timeout_t*)id;
	r = aio_timeout_stop(id);
	free(t);
	return r;
}
