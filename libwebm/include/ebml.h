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

int32_t ebml_read_id(struct ebml_t* ebml);

int64_t ebml_read_size(struct ebml_t* ebml);

int ebml_read_header(struct ebml_t* ebml, struct ebml_header_t* header);

#ifdef __cplusplus
}
#endif
#endif /* !_ebml_h_ */
