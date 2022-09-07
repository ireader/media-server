#include "sip-timer.h"
#include "aio-timeout.h"
#include "sys/system.h"
#include "sys/thread.h"
#include <stdlib.h>

static pthread_t s_threads[2];
static int s_running;

static int STDCALL sip_timer_run(void* param)
{
	volatile int* running = (int*)param;
	while (*running)
	{
		aio_timeout_process();
		system_sleep(5);
	}
	return 0;
}

void sip_timer_init(void)
{
	int i;
	s_running = 1;
	for(i = 0; i < sizeof(s_threads)/sizeof(s_threads[0]); i++)
	{
		thread_create(s_threads+i, sip_timer_run, &s_running);
	}
}

void sip_timer_cleanup(void)
{
	int i;

	s_running = 0;
	for (i = 0; i < sizeof(s_threads) / sizeof(s_threads[0]); i++)
	{
		thread_destroy(s_threads[i]);
	}
}

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
