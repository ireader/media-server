#ifndef _ebml_h_
#define _ebml_h_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ebml_element_type_t
{
    EBML_TYPE_UNKNOWN,
    EBML_TYPE_INT, // Signed Integer Element [0-8]
    EBML_TYPE_UINT, // Unsigned Integer Element [0-8]
    EBML_TYPE_FLOAT, // Float Element (0/4/8)
    EBML_TYPE_STRING, // ASCII String Element [0-VINTMAX]
    EBML_TYPE_UTF8, // UTF-8 Element [0-VINTMAX]
    EBML_TYPE_DATE, // Date Element [0-8]
    EBML_TYPE_MASTER, // Master Element [0-VINTMAX]
    EBML_TYPE_BINARY, // Binary Element [0-VINTMAX]
};

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

/// @return size with prefix bytes
unsigned int ebml_size_length(uint64_t size);

/// @return unsigned integer value bytes
unsigned int ebml_uint_length(uint64_t v);

/// @param[out] buf value writer buffer
/// @return value length (same as ebml_uint_length)
unsigned int ebml_write_uint(uint8_t buf[8], uint64_t v);

/// Write EBML element id + size
/// @param[out] buf write buffer
/// @param[in] id element id
/// @param[in] size element size
/// @param[in] bytes size write bytes, 0-ebml_size_length(size)
/// @return write length
unsigned int ebml_write_element(uint8_t buf[12], uint32_t id, uint64_t size, unsigned int bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_ebml_h_ */
