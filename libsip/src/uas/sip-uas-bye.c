#include "sip-uas-transaction.h"

int sip_uas_onbye(struct sip_uas_transaction_t* t, const struct sip_message_t* req, void* param)
{
	char ptr[256];
	struct cstring_t id;

	sip_dialog_id_with_message(&id, req, ptr, sizeof(ptr), 1);

	//session = dialog ? dialog->session : NULL; // get session before dialog remove
	//if (0 != sip_dialog_remove(t->agent, dialog))
	//{
	//	// 481 Call/Transaction Does Not Exist
	//	return sip_uas_transaction_noninvite_reply(t, 481, NULL, 0, param);
	//}

	// The UAS MUST still respond to any pending requests received for that 
	// dialog. It is RECOMMENDED that a 487 (Request Terminated) response be 
	// generated to those pending requests.
	return t->handler->onbye(param, req, t, &id);
}
