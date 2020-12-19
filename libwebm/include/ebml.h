#ifndef _ebml_h_
#define _ebml_h_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ebml_t
{
	uint8_t* ptr;
	size_t off;
	size_t len;
	int err;
};

// https://github.com/ietf-wg-cellar/ebml-specification/blob/master/specification.markdown#ebml-header-elements
struct ebml_header_t
{
	unsigned int version; // default 1
	unsigned int read_version; // default 1
	unsigned int max_id_length; // default 4
	unsigned int max_size_length; // default 8

	char* doc_type;
	unsigned int doc_type_version; // default 1
	unsigned int doc_type_read_version; // default 1
};

#ifdef __cplusplus
}
#endif
#endif /* !_ebml_h_ */
