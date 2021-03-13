#ifndef _sip_timer_h_
#define _sip_timer_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sip_timer_handle)(void* usrptr);

//struct sip_timer_t
//{
//	/// Start a timer
//	/// @param[in] timeout milliseconds
//	/// @param[in] usrptr user-defined pointer
//	/// @return timer id(used by stop)
//	void* (*start)(void* timer, int timeout, sip_timer_handle handler, void* usrptr);
//
//	/// Cancel timer
//	/// @param[in] id start return timer id
//	void (*stop)(void* timer, void* id);
//};

typedef void* sip_timer_t;

void sip_timer_init(void);
void sip_timer_cleanup(void);

sip_timer_t sip_timer_start(int timeout, sip_timer_handle handler, void* usrptr);
int sip_timer_stop(sip_timer_t* id);

#ifdef __cplusplus
}
#endif
#endif /* !_sip_timer_h_ */
