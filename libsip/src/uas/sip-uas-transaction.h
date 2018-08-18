#ifndef _sip_uas_transaction_h_
#define _sip_uas_transaction_h_

#include "sip-message.h"
#include "sip-header.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "list.h"

#define UDP_PACKET_SIZE 1440

typedef int(*sip_uas_transaction_handler)(void* param);

enum
{
	SIP_UAS_TRANSACTION_INIT,
	SIP_UAS_TRANSACTION_TRYING,
	SIP_UAS_TRANSACTION_PROCEEDING,
	SIP_UAS_TRANSACTION_COMPLETED,
	SIP_UAS_TRANSACTION_COMFIRMED,
	SIP_UAS_TRANSACTION_TERMINATED,
};

struct sip_uas_t;
struct sip_uas_transaction_t
{
	struct list_head link;
	locker_t locker;

	struct sip_message_t* msg;
	uint8_t data[UDP_PACKET_SIZE];
	int size;
	int retransmission; // default 0

	int32_t status;
	int retries;
	int t2; // 64*T1-invite, 4s-noninvite
	void* timera; // retransmission timer
	void* timerb; // timeout
//	void* timerd;
	void* timerk;

	struct sip_uas_t* uas;
	sip_uas_transaction_handler handler;
	void* param;
};

struct sip_uas_transaction_t* sip_uas_transaction_create(struct sip_uas_t* uas, const struct sip_message_t* msg);
int sip_uas_transaction_destroy(struct sip_uas_transaction_t* t);

int sip_uas_transaction_send(struct sip_uas_transaction_t* t);

int sip_uas_transaction_invite_input(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req);
int sip_uas_transaction_noninvite_input(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req);

#endif /* !_sip_uas_transaction_h_ */
