#include "sip-uas-transaction.h"

int sip_uas_onbye(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req, void* param)
{
	//dialog = sip_dialog_find(&t->uas->dialogs, req);
	if (!dialog)
	{
		// 481 Call/Transaction Does Not Exist
		return sip_uas_transaction_noninvite_reply(t, 481, NULL, 0, param);
	}

	if (0 != sip_dialog_remove(t->agent, dialog))
	{
		// 481 Call/Transaction Does Not Exist
		return sip_uas_transaction_noninvite_reply(t, 481, NULL, 0, param);
	}

	// The UAS MUST still respond to any pending requests received for that 
	// dialog. It is RECOMMENDED that a 487 (Request Terminated) response be 
	// generated to those pending requests.
	return t->handler->onbye(param, req, t, dialog ? dialog->session : NULL);
}
