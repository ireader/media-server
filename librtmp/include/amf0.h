#ifndef _amf0_h_
#define _amf0_h_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum AMFDataType
{
	AMF_NUMBER = 0x00,
	AMF_BOOLEAN,
	AMF_STRING,
	AMF_OBJECT,
	AMF_MOVIECLIP,
	AMF_NULL,
	AMF_UNDEFINED,
	AMF_REFERENCE,
	AMF_ECMA_ARRAY,
	AMF_OBJECT_END,
	AMF_STRICT_ARRAY,
	AMF_DATE,
	AMF_LONG_STRING,
	AMF_UNSUPPORTED,
	AMF_RECORDSET,
	AMF_XML_DOCUMENT,
	AMF_TYPED_OBJECT,
	AMF_AVMPLUS_OBJECT,
};

char* AMFWriteBoolean(char* out, size_t size, int value);
char* AMFWriteDouble(char* out, size_t size, double value);
char* AMFWriteString(char* out, size_t size, const char* string, size_t length);

char* AMFWriteNamedString(char* out, size_t size, const char* name, size_t length, const char* value, size_t length2);
char* AMFWriteNamedDouble(char* out, size_t size, const char* name, size_t length, double value);
char* AMFWriteNamedBoolean(char* out, size_t size, const char* name, size_t length, int value);

const char* AMFReadBoolean(const char* in, size_t size, int* value);
const char* AMFReadDouble(const char* in, size_t size, double* value);
const char* AMFReadString(const char* in, size_t size, int isLongString, char* string, size_t* length);

#ifdef __cplusplus
}
#endif
#endif /* !_amf0_h_ */
