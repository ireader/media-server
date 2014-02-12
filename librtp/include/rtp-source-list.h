#ifndef _rtp_source_list_h_
#define _rtp_source_list_h_

#include "rtp-source.h"

void* rtp_source_list_create();
void rtp_source_list_destroy(void* members);

int rtp_source_list_count(void* members);
struct rtp_source* rtp_source_list_get(void* members, int index);

struct rtp_source* rtp_source_list_find(void* members, unsigned int ssrc);

int rtp_source_list_add(void* members, struct rtp_source* source);
int rtp_source_list_delete(void* members, unsigned int ssrc);

#endif /* !_rtp_source_list_h_ */
