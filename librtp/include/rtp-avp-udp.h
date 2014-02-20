#ifndef _librtp_avp_udp_h_
#define _librtp_avp_udp_h_

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	#define DLL_EXPORT_API __declspec(dllexport)
	#define DLL_IMPORT_API __declspec(dllimport)
#else
	#if __GNUC__ >= 4
		#define DLL_EXPORT_API __attribute__((visibility ("default")))
		#define DLL_IMPORT_API
	#else
		#define DLL_EXPORT_API
		#define DLL_IMPORT_API
	#endif
#endif

#if defined(LIBRTP_EXPORTS)
	#define RTP_API DLL_EXPORT_API
#else
	#define RTP_API DLL_IMPORT_API
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#include "rtp-queue.h"
#include "h264-source.h"

/// initialization/cleanup
RTP_API int rtp_avp_init(void);
RTP_API int rtp_avp_cleanup(void);

/// create rtp receiver
/// @param[in] rtp rtp socket value
/// @param[in] rtcp rtcp socket value
/// @param[in] queue rtp queue(create by rtp_queue_create)
/// @return NULL-error, other-object
RTP_API void* rtp_avp_udp_create(int rtp, int rtcp, void* queue);
RTP_API int rtp_avp_udp_destroy(void* udp);

/// start/pause receive
/// @param[in] udp (create by rtp_avp_udp_create)
RTP_API int rtp_avp_udp_start(void* udp);
RTP_API int rtp_avp_udp_pause(void* udp);
RTP_API int rtp_avp_udp_stop(void* udp);

#if defined(__cplusplus)
}
#endif
#endif /* !_librtp_avp_udp_h_ */
