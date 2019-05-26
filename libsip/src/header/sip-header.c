#include "sip-header.h"
#include <stdlib.h>
#include <string.h>

char* cstring_clone(char* ptr, const char* end, struct cstring_t* clone, const char* s, size_t n)
{
	size_t remain;
	remain = end - ptr;

	clone->p = ptr;
	clone->n = remain >= n ? n : remain;

	memcpy(ptr, s, clone->n);
	ptr += clone->n;
	if (ptr < end) 
		*ptr++ = '\0';
	return ptr;
}

char* sip_uri_clone(char* ptr, const char* end, struct sip_uri_t* clone, const struct sip_uri_t* uri)
{
	int n, r;
	n = sip_uri_write(uri, ptr, end);
	r = sip_header_uri(ptr, ptr + n, clone);
	return 0 == r ? ptr + n : (char*)end;
}

char* sip_via_clone(char* ptr, const char* end, struct sip_via_t* clone, const struct sip_via_t* via)
{
	int n, r;
	n = sip_via_write(via, ptr, end);
	r = sip_header_via(ptr, ptr + n, clone);
	return 0 == r ? ptr + n : (char*)end;
}

char* sip_contact_clone(char* ptr, const char* end, struct sip_contact_t* clone, const struct sip_contact_t* contact)
{
	int n, r;
	n = sip_contact_write(contact, ptr, end);
	r = sip_header_contact(ptr, ptr + n, clone);
	return 0 == r ? ptr + n : (char*)end;
}
