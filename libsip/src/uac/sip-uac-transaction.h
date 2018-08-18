#ifndef _sip_uac_transaction_h_
#define _sip_uac_transaction_h_

#include "sip-uac.h"
#include "sys/locker.h"
#include "list.h"

#define UDP_PACKET_SIZE 1440

enum
{
	SIP_UAC_TRANSACTION_CALLING,
	SIP_UAC_TRANSACTION_TRYING = SIP_UAC_TRANSACTION_CALLING,
	SIP_UAC_TRANSACTION_PROCEEDING,
	SIP_UAC_TRANSACTION_COMPLETED,
	SIP_UAC_TRANSACTION_ACCEPTED, // rfc6026
	SIP_UAC_TRANSACTION_TERMINATED,
};

struct sip_uac_t;
struct sip_uac_transaction_t
{
	struct list_head link;
	locker_t locker;
	int32_t ref;

	struct sip_message_t* req;
	struct sip_message_t* reply;
	uint8_t data[UDP_PACKET_SIZE];
	int size;
	int retransmission; // default 0

	int status;
	int retries;
	int t2; // 64*T1-invite, 4s-noninvite
	void* timera; // retransmission timer
	void* timerb; // timeout
//	void* timerd;
	void* timerk;

	struct sip_uac_t* uac;
	sip_uac_oninvite oninvite;
	sip_uac_onreply onreply;
	void* param;
};

struct sip_uac_transaction_t* sip_uac_transaction_create(struct sip_uac_t* uac);
int sip_uac_transaction_addref(struct sip_uac_transaction_t* t);
int sip_uac_transaction_release(struct sip_uac_transaction_t* t);

int sip_uac_transaction_send(struct sip_uac_transaction_t* t);

int sip_uac_transaction_invite_input(struct sip_uac_transaction_t* t, const struct sip_message_t* reply);
int sip_uac_transaction_noninvite_input(struct sip_uac_transaction_t* t, const struct sip_message_t* reply);

#endif /* !_sip_uac_transaction_h_ */
