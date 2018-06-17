#include "sip-header.h"
#include <stdio.h>

/*
CSeq = "CSeq" HCOLON 1*DIGIT LWS Method
*/

int sip_header_cseq(const char* s, const char* end, struct sip_cseq_t* cseq)
{
	char* p;

	cseq->id = (uint32_t)strtoul(s, &p, 10);

	cseq->method.p = p;
	cseq->method.n = end - p;
	cstrtrim(&cseq->method, " \t\r\n");

	return 0;
}

int sip_cseq_write(const struct sip_cseq_t* cseq, char* data, const char* end)
{
	char* p;
	if (!cstrvalid(&cseq->method))
		return -1;

	p = data;
	if (p < end)
		snprintf(p, end - p, "%ul ", (unsigned long)cseq->id);

	if (p < end)
		p += cstrcpy(&cseq->method, p, end - p);

	return p - data;
}

#if defined(DEBUG) || defined(_DEBUG)
void sip_header_cseq_test(void)
{
	const char* s;
	struct sip_cseq_t cseq;

	s = "314159 INVITE";
	assert(0 == sip_header_cseq(s, s + strlen(s), &cseq));
	assert(0 == cstrcmp(&cseq.method, "INVITE") && cseq.id == 314159);
}
#endif
