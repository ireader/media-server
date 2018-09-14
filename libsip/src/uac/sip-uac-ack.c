#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"
#include <stdio.h>

int sip_uac_ack(struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog)
{
	int r;
	char via[256];
	char local[128];
	char remote[256]; // destination/router
	char protocol[16];
	struct sip_message_t* req;

	req = sip_message_create();
	if (0 != sip_message_init2(req, SIP_METHOD_ACK, dialog))
	{
		sip_message_destroy(req);
		return -1;
	}

	// router
	r = sip_message_get_next_hop(req, remote, sizeof(remote));
	if (0 != r)
		return r;

	r = t->transport->via(t->transportptr, remote, protocol, local);
	if (0 != r)
		return r;

	// Via: SIP/2.0/UDP erlang.bell-telephone.com:5060;branch=z9hG4bK87asdks7
	// Via: SIP/2.0/UDP first.example.com:4000;ttl=16;maddr=224.2.0.1;branch=z9hG4bKa7c6a8dlze.1
	r = snprintf(via, sizeof(via), "SIP/2.0/%s %s;branch=%s%p", protocol, local, SIP_BRANCH_PREFIX, t);
	if (r < 0 || r >= sizeof(via))
		return -1; // ENOMEM
	sip_message_add_header(req, "Via", via);

	// message
	t->size = sip_message_write(t->req, t->data, sizeof(t->data));
	if (t->size < 0 || t->size >= sizeof(t->data))
	{
		sip_uac_transaction_release(t);
		return -1;
	}

	return t->transport->send(t->transportptr, t->data, t->size);
}
