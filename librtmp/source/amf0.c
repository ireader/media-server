#include "amf0.h"
#include <stdint.h>

static char* AMFWriteInt16(char* out, size_t size, uint16_t value)
{
	assert(size >= 2);
	out[0] = value >> 8;
	out[1] = value & 0xFF;
	return out + 2;
}

static char* AMFWriteInt32(char* out, size_t size, uint32_t value)
{
	assert(size >= 4);
	out[0] = value >> 24;
	out[1] = value >> 16;
	out[2] = value >> 8;
	out[3] = value & 0xFF;
	return out + 4;
}

static char* AMFWriteString16(char* out, size_t size, const char* string, size_t length)
{
	assert(size >= 2 + length);
	out = AMFWriteInt16(out, size, (uint16_t)length);
	memcpy(out, string, length);
	return out + length;
}

static char* AMFWriteString32(char* out, size_t size, const char* string, size_t length)
{
	assert(size >= 4 + length);
	out = AMFWriteInt32(out, size, (uint32_t)length);
	memcpy(out, string, length);
	return out + length;
}

char* AMFWriteBoolean(char* out, size_t size, int value)
{
	if (size < 2) return NULL;

	out[0] = AMF_BOOLEAN;
	out[1] = 0 == value ? 0 : 1;
	return out + 2;
}

char* AMFWriteDouble(char* out, size_t size, double value)
{
	const unsigned char* p;
	if (size < 9) return NULL;

	assert(8 == sizeof(double));
	p = (const unsigned char*)&value;
	*out++ = AMF_NUMBER;
	memcpy(out, &value, sizeof(double));
	return out + 8;
}

char* AMFWriteString(char* out, size_t size, const char* string, size_t length)
{
	if (size < 1 + (length < 65536 ? 2 : 4) + length || length > UINT32_MAX) return NULL;

	if (length < 65536)
	{
		*out++ = AMF_STRING;
		AMFWriteString16(out, size - 1, string, length);
	}
	else
	{
		*out++ = AMF_LONG_STRING;
		AMFWriteString32(out, size - 1, string, length);
	}
	return out + length;
}

static const char* AMFReadInt16(const char* in, size_t size, uint16_t* value)
{
	if (size < 2) return NULL;

	*value = in[0] << 8;
	*value |= in[1];
	return in + 2;
}

static const char* AMFReadInt32(const char* in, size_t size, uint32_t* value)
{
	if (size < 4) return NULL;

	*value = in[0] << 24;
	*value |= in[1] << 16;
	*value |= in[2] << 8;
	*value |= in[3];
	return in + 4;
}

const char* AMFReadBoolean(const char* in, size_t size, int* value)
{
	if (size < 1) return NULL;
	*value = in[0];
	return in + 1;
}

const char* AMFReadDouble(const char* in, size_t size, double* value)
{
	unsigned char* p;
	if (size < sizeof(double)) return NULL;
	memcpy(value, in, sizeof(double));
	return in + sizeof(double);
}

const char* AMFReadString(const char* in, size_t size, int isLongString, char* string, size_t* length)
{ 
	if (isLongString)
		in = AMFReadInt16(in, size, (uint16_t*)length);
	else
		in = AMFReadInt32(in, size, (uint32_t*)length);

	if (NULL == in || size < *length) return NULL;

	memcpy(string, in, *length);
	return in + *length;
}

char* AMFWriteNamedString(char* out, size_t size, const char* name, size_t length, const char* value, size_t length2)
{
	if (length + 2 + length2 + 3 > size)
		return NULL;

	out = AMFWriteString16(out, size, name, length);

	return AMFWriteString(out, size - 2 - length, value, length2);
}

char* AMFWriteNamedDouble(char* out, size_t size, const char* name, size_t length, double value)
{
	if (length + 2 + 8 + 1 > size)
		return NULL;

	out = AMFWriteString16(out, size, name, length);

	return AMFWriteDouble(out, size - 2 - length, value);
}

char* AMFWriteNamedBoolean(char* out, size_t size, const char* name, size_t length, int value)
{
	if (length + 2 + 2 > size)
		return NULL;

	out = AMFWriteString16(out, size, name, length);

	return AMFWriteBoolean(out, size - 2 - length, value);
}
