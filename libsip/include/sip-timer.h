#ifndef _sip_timer_h_
#define _sip_timer_h_

typedef int (*sip_timer_handle)(void* id, void* usrptr);

struct sip_timer_t
{
	/// Start a timer
	/// @param[in] timeout milliseconds
	/// @param[in] usrptr user-defined pointer
	/// @return timer id(used by stop)
	void* (*start)(void* timer, int timeout, sip_timer_handle handler, void* usrptr);

	/// Cancel timer
	/// @param[in] id start return timer id
	void (*stop)(void* timer, void* id);
};

#endif /* !_sip_timer_h_ */
