#ifndef _sip_uac_h_
#define _sip_uac_h_

#include "sip-timer.h"
#include "sip-header.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "list.h"

#define UDP_PACKET_SIZE 1440

enum
{
	SIP_UAC_TRANSACTION_TRYING,
	SIP_UAC_TRANSACTION_CALLING,
	SIP_UAC_TRANSACTION_PROCEEDING,
	SIP_UAC_TRANSACTION_COMPLETED,
	SIP_UAC_TRANSACTION_TERMINATED,
};

typedef int(*sip_uac_transaction_handler)(void* param);

struct sip_uac_transaction_t
{
	struct list_head link;
	locker_t locker;

	struct sip_message_t* msg;
	uint8_t data[UDP_PACKET_SIZE];
	int size;
	int retransmission; // default 0

	int status;
	void* timera;
	void* timerb;
	void* timerd;
	
	struct sip_uac_t* uac;
	sip_uac_transaction_handler handler;
	void* param;
};

struct sip_uac_t
{
	int32_t ref;
	locker_t locker;

	struct sip_transport_t* transport;
	void* transportptr;

	struct sip_timer_t timer;
	void* timerptr;

	struct list_head transactions; // transaction layer handler
	struct list_head dialogs; // early or confirmed dialogs
};

struct sip_uac_transaction_t* sip_uac_transaction_create(struct sip_uac_t* uac, const struct sip_message_t* msg);
int sip_uac_transaction_destroy(struct sip_uac_transaction_t* t);

#endif /* !_sip_uac_h_ */
