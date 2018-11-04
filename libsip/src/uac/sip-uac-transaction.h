#ifndef _sip_uac_transaction_h_
#define _sip_uac_transaction_h_

#include "sip-uac.h"
#include "sip-timer.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "sys/locker.h"
#include "list.h"

#define UDP_PACKET_SIZE 1440

enum
{
	SIP_UAC_TRANSACTION_CALLING,
	SIP_UAC_TRANSACTION_TRYING = SIP_UAC_TRANSACTION_CALLING,
	SIP_UAC_TRANSACTION_PROCEEDING,
	SIP_UAC_TRANSACTION_COMPLETED,
	SIP_UAC_TRANSACTION_ACCEPTED, // rfc6026 7.2. (p9)
	SIP_UAC_TRANSACTION_TERMINATED,
};

struct sip_uac_t;
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
	void* timera; // retransmission timer
	void* timerb; // timeout
//	void* timerd;
	void* timerk; // wait for all duplicate-reply(ack) message

	struct sip_uac_t* uac;
	int (*onhandle)(struct sip_uac_transaction_t* t, const struct sip_message_t* reply);
	sip_uac_oninvite oninvite;
	sip_uac_onreply onreply;
	void* param;
	
	struct sip_transport_t transport;
//	sip_uac_onsend onsend;
	void* transportptr;
};

struct sip_uac_transaction_t* sip_uac_transaction_create(struct sip_uac_t* uac, struct sip_message_t* req);
int sip_uac_transaction_addref(struct sip_uac_transaction_t* t);
int sip_uac_transaction_release(struct sip_uac_transaction_t* t);

int sip_uac_transaction_send(struct sip_uac_transaction_t* t);

int sip_uac_transaction_invite_input(struct sip_uac_transaction_t* t, const struct sip_message_t* reply);
int sip_uac_transaction_noninvite_input(struct sip_uac_transaction_t* t, const struct sip_message_t* reply);

// wait for all inflight reply
int sip_uac_transaction_timewait(struct sip_uac_transaction_t* t, int timeout);

int sip_uac_transaction_via(struct sip_uac_transaction_t* t, char via[1024], char contact[1024]);

int sip_uac_add_transaction(struct sip_uac_t* uac, struct sip_uac_transaction_t* t);
int sip_uac_del_transaction(struct sip_uac_t* uac, struct sip_uac_transaction_t* t);

void* sip_uac_start_timer(struct sip_uac_t* uac, struct sip_uac_transaction_t* t, int timeout, sip_timer_handle handler);
void sip_uac_stop_timer(struct sip_uac_t* uac, struct sip_uac_transaction_t* t, void* id);

int sip_uac_ack(struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int newtransaction);

#endif /* !_sip_uac_transaction_h_ */
