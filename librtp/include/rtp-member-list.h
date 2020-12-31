#ifndef _rtp_member_list_h_
#define _rtp_member_list_h_

#include "rtp-member.h"

void* rtp_member_list_create(void);
void rtp_member_list_destroy(void* members);

int rtp_member_list_count(void* members);
struct rtp_member* rtp_member_list_get(void* members, int index);

struct rtp_member* rtp_member_list_find(void* members, uint32_t ssrc);

int rtp_member_list_add(void* members, struct rtp_member* source);
int rtp_member_list_delete(void* members, uint32_t ssrc);

#endif /* !_rtp_member_list_h_ */
