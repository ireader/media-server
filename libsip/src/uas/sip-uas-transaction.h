#ifndef _sip_uas_transaction_h_
#define _sip_uas_transaction_h_

#include "sys/locker.h"
#include "list.h"

#define UDP_PACKET_SIZE 1440

typedef int(*sip_uas_transaction_handler)(void* param);

enum
{
	SIP_UAC_TRANSACTION_CALLING,
	SIP_UAC_TRANSACTION_TRYING = SIP_UAC_TRANSACTION_CALLING,
	SIP_UAC_TRANSACTION_PROCEEDING,
	SIP_UAC_TRANSACTION_COMPLETED,
	SIP_UAC_TRANSACTION_ACCEPTED, // rfc6026
	SIP_UAC_TRANSACTION_TERMINATED,
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

	int status;
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

int sip_uas_transaction_invite_input(struct sip_uas_transaction_t* t, const struct sip_message_t* reply);
int sip_uas_transaction_noninvite_input(struct sip_uas_transaction_t* t, const struct sip_message_t* reply);

#endif /* !_sip_uas_transaction_h_ */
