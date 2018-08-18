#include "sip-uas-transaction.h"

int sip_uas_onbye(struct sip_uas_transaction_t* t, const struct sip_message_t* req)
{
	int r;
	struct sip_dialog_t* dialog;

	dialog = sip_dialog_find(&t->uas->dialogs, req);
	if (!dialog)
	{
		// 481 Call/Transaction Does Not Exist
		return sip_uas_transaction_noninvite_reply(t, 481, NULL, 0);
	}

	// The UAS MUST still respond to any pending requests received for that 
	// dialog. It is RECOMMENDED that a 487 (Request Terminated) response be 
	// generated to those pending requests.
	r =  t->uas->handler->onbye(t->uas->param, t, dialog->session);

	list_remove(&dialog->link);
	return r;
}
