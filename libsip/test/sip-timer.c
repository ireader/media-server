#include "sip-timer.h"
#include "aio-timeout.h"
#include <stdlib.h>

sip_timer_t sip_timer_start(int timeout, sip_timer_handle handler, void* usrptr)
{
	struct aio_timeout_t* t;
	t = calloc(1, sizeof(struct aio_timeout_t));
	if (0 == aio_timeout_start(t, timeout, handler, usrptr))
		return t;
	free(t);
	return NULL;
}

int sip_timer_stop(sip_timer_t* id)
{
	int r;
	struct aio_timeout_t* t;
	if (NULL == id || NULL == *id)
		return -1;
	t = (struct aio_timeout_t*)*id;
	r = aio_timeout_stop(t);
	free(t);
	*id = NULL;
	return r;
}
