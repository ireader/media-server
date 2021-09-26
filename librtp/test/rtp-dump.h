#ifndef _rtp_dump_h_
#define _rtp_dump_h_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rtpdump_t;

struct rtpdump_t* rtpdump_open(const char* file, int flags);

int rtpdump_close(struct rtpdump_t* dump);

int rtpdump_read(struct rtpdump_t* dump, uint32_t* clock, void* data, int bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_rtp_dump_h_ */
