//#include "sys/sock.h"
#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"
//#include "port/ip-route.h"
//#include "uri-parse.h"

struct sip_uac_transaction_t* sip_uac_register(struct sip_uac_t* uac, const char* name, const char* registrar, int seconds, sip_uac_onreply onregister, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if (0 != sip_message_init(req, SIP_METHOD_REGISTER, registrar ? registrar : name, name, name))
	{
		sip_message_destroy(req);
		return NULL;
	}

	sip_message_add_header_int(req, "Expires", seconds);

	t = sip_uac_transaction_create(uac, req);
	t->onreply = onregister;
	t->param = param;
	return t;
}

//int sip_uac_via(const char* host, char via[65])
//{
//	int r;
//	char ip[65];
//	u_short port;
//	struct uri_t* uri;
//	struct sockaddr_storage ss;
//	socklen_t len;
//
//	len = sizeof(ss);
//	memset(&ss, 0, sizeof(ss));
//
//	uri = uri_parse(host, strlen(host));
//	if (!uri)
//		return -1; // invalid uri
//
//	// TODO: sips port
//	r = socket_addr_from(&ss, &len, uri->host, uri->port ? uri->port : SIP_PORT);
//	if (0 == r)
//	{
//		socket_addr_to((struct sockaddr*)&ss, len, ip, &port);
//		r = ip_route_get(ip, via);
//	}
//
//	uri_free(uri);
//	return r;
//}
