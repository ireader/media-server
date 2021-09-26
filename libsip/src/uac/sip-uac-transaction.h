#ifndef _sip_uac_transaction_h_
#define _sip_uac_transaction_h_

#include "sip-uac.h"
#include "sip-timer.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "list.h"

#define UDP_PACKET_SIZE (4*1024) //1440

enum
{
	SIP_UAC_TRANSACTION_CALLING,
	SIP_UAC_TRANSACTION_TRYING = SIP_UAC_TRANSACTION_CALLING,
	SIP_UAC_TRANSACTION_PROCEEDING,
	SIP_UAC_TRANSACTION_COMPLETED,
	SIP_UAC_TRANSACTION_ACCEPTED, // rfc6026 7.2. (p9)
	SIP_UAC_TRANSACTION_TERMINATED,

	SIP_UAC_TRANSACTION_ACCEPTED_UNACK, // wait for user ack, internal state only
	SIP_UAC_TRANSACTION_ACCEPTED_ACKED, // user acked, internal state only
};

struct sip_agent_t;
struct sip_uac_transaction_t
{
	struct list_head link;
	locker_t locker;
	int32_t ref;

	// valid only in [sip_uas_input, sip_uas_reply]
	// create in sip_uas_input, destroy in sip_uas_reply
	struct sip_message_t* req;
	uint8_t data[UDP_PACKET_SIZE];
	int size;
	int reliable; // udp-0, other-1
	int retransmission; // default 0

	int status;
	int retries;
	int t2; // 64*T1-invite, 4s-non-invite
	sip_timer_t timera; // retransmission timer(timer E)
	sip_timer_t timerb; // timeout(timer F)
	sip_timer_t timerd; // wait for all duplicate-reply(ack) message(timer K)

	struct sip_agent_t* agent;
	struct sip_dialog_t* dialog;
//	int (*onhandle)(struct sip_uac_transaction_t* t, const struct sip_message_t* reply);
	sip_uac_onsubscribe onsubscribe;
	sip_uac_oninvite oninvite;
	sip_uac_onreply onreply;
	void* param;
    sip_transaction_ondestroy ondestroy;
    void* ondestroyparam;
	
	struct sip_transport_t transport;
//	sip_uac_onsend onsend;
	void* transportptr;
};

struct sip_uac_transaction_t* sip_uac_transaction_create(struct sip_agent_t* sip, struct sip_message_t* req);
//int sip_uac_transaction_addref(struct sip_uac_transaction_t* t);
//int sip_uac_transaction_release(struct sip_uac_transaction_t* t);

int sip_uac_transaction_send(struct sip_uac_transaction_t* t);

int sip_uac_transaction_invite_input(struct sip_uac_transaction_t* t, const struct sip_message_t* reply);
int sip_uac_transaction_noninvite_input(struct sip_uac_transaction_t* t, const struct sip_message_t* reply);

// calling/trying + proceeding timeout
int sip_uac_transaction_timeout(struct sip_uac_transaction_t* t, int timeout);

// wait for all inflight reply
int sip_uac_transaction_timewait(struct sip_uac_transaction_t* t, int timeout);

int sip_uac_transaction_via(struct sip_uac_transaction_t* t, char *via, int nvia, char *contact, int nconcat);

sip_timer_t sip_uac_start_timer(struct sip_agent_t* sip, struct sip_uac_transaction_t* t, int timeout, sip_timer_handle handler);
void sip_uac_stop_timer(struct sip_agent_t* sip, struct sip_uac_transaction_t* t, sip_timer_t* id);

int sip_uac_ack_3456xx(struct sip_uac_transaction_t* t, const struct sip_message_t* reply, struct sip_dialog_t* dialog);

int sip_uac_notify_onreply(struct sip_uac_transaction_t* t, const struct sip_message_t* reply);
int sip_uac_subscribe_onreply(struct sip_uac_transaction_t* t, const struct sip_message_t* reply);

#endif /* !_sip_uac_transaction_h_ */
