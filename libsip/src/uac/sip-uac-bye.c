#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"

struct sip_uac_transaction_t* sip_uac_dialog_custom(struct sip_agent_t* sip, const char* method, struct sip_dialog_t* dialog, sip_uac_onreply onreply, void* param);

struct sip_uac_transaction_t* sip_uac_bye(struct sip_agent_t* sip, struct sip_dialog_t* dialog, sip_uac_onreply onbye, void* param)
{
	return sip_uac_dialog_custom(sip, SIP_METHOD_BYE, dialog, onbye, param);
}
