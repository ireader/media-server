#include "amf0.h"
#include <stddef.h>
#include <memory.h>
#include <assert.h>

static uint8_t* AMFWriteInt16(uint8_t* ptr, const uint8_t* end, uint16_t value)
{
	assert(end - ptr >= 2);
	ptr[0] = value >> 8;
	ptr[1] = value & 0xFF;
	return ptr + 2;
}

static uint8_t* AMFWriteInt32(uint8_t* ptr, const uint8_t* end, uint32_t value)
{
	assert(end - ptr >= 4);
	ptr[0] = (uint8_t)(value >> 24);
	ptr[1] = (uint8_t)(value >> 16);
	ptr[2] = (uint8_t)(value >> 8);
	ptr[3] = (uint8_t)(value & 0xFF);
	return ptr + 4;
}

static uint8_t* AMFWriteString16(uint8_t* ptr, const uint8_t* end, const char* string, size_t length)
{
	assert(ptr + 2 + length <= end);
	ptr = AMFWriteInt16(ptr, end, (uint16_t)length);
	memcpy(ptr, string, length);
	return ptr + length;
}

static uint8_t* AMFWriteString32(uint8_t* ptr, const uint8_t* end, const char* string, size_t length)
{
	assert(ptr + 4 + length <= end);
	ptr = AMFWriteInt32(ptr, end, (uint32_t)length);
	memcpy(ptr, string, length);
	return ptr + length;
}

uint8_t* AMFWriteNull(uint8_t* ptr, const uint8_t* end)
{
	if (!ptr || end - ptr < 1) return NULL;

	*ptr++ = AMF_NULL;
	return ptr;
}

uint8_t* AMFWriteObject(uint8_t* ptr, const uint8_t* end)
{
	if (!ptr || end - ptr < 1) return NULL;

	*ptr++ = AMF_OBJECT;
	return ptr;
}

uint8_t* AMFWriteObjectEnd(uint8_t* ptr, const uint8_t* end)
{
	if (!ptr || end - ptr < 3) return NULL;

	/* end of object - 0x00 0x00 0x09 */
	*ptr++ = 0;
	*ptr++ = 0;
	*ptr++ = AMF_OBJECT;
	return ptr;
}

uint8_t* AMFWriteBoolean(uint8_t* ptr, const uint8_t* end, uint8_t value)
{
	if (!ptr || end - ptr < 2) return NULL;

	ptr[0] = AMF_BOOLEAN;
	ptr[1] = 0 == value ? 0 : 1;
	return ptr + 2;
}

uint8_t* AMFWriteDouble(uint8_t* ptr, const uint8_t* end, double value)
{
	if (!ptr || end - ptr < 9) return NULL;

	assert(8 == sizeof(double));
	*ptr++ = AMF_NUMBER;
	memcpy(ptr, &value, sizeof(double));
	return ptr + 8;
}

uint8_t* AMFWriteString(uint8_t* ptr, const uint8_t* end, const char* string, size_t length)
{
	if (!ptr || ptr + 1 + (length < 65536 ? 2 : 4) + length > end || length > UINT32_MAX) return NULL;

	if (length < 65536)
	{
		*ptr++ = AMF_STRING;
		AMFWriteString16(ptr, end, string, length);
	}
	else
	{
		*ptr++ = AMF_LONG_STRING;
		AMFWriteString32(ptr, end, string, length);
	}
	return ptr + length;
}

static const uint8_t* AMFReadInt16(const uint8_t* ptr, const uint8_t* end, uint16_t* value)
{
	if (!ptr || end - ptr < 2) return NULL;

	*value = ptr[0] << 8;
	*value |= ptr[1];
	return ptr + 2;
}

static const uint8_t* AMFReadInt32(const uint8_t* ptr, const uint8_t* end, uint32_t* value)
{
	if (end - ptr < 4) return NULL;

	*value = ptr[0] << 24;
	*value |= ptr[1] << 16;
	*value |= ptr[2] << 8;
	*value |= ptr[3];
	return ptr + 4;
}

const uint8_t* AMFReadNull(const uint8_t* ptr, const uint8_t* end)
{
	assert(ptr && end);
	return ptr;
}

const uint8_t* AMFReadBoolean(const uint8_t* ptr, const uint8_t* end, uint8_t* value)
{
	if (end - ptr < 1) return NULL;
	*value = ptr[0];
	return ptr + 1;
}

const uint8_t* AMFReadDouble(const uint8_t* ptr, const uint8_t* end, double* value)
{
	if (end - ptr < 8) return NULL;
	memcpy(value, ptr, sizeof(double));
	return ptr + 8;
}

const uint8_t* AMFReadString(const uint8_t* ptr, const uint8_t* end, int isLongString, char* string, size_t length)
{ 
	uint32_t len = 0xFFFFFFFF;
	if (isLongString)
		ptr = AMFReadInt16(ptr, end, (uint16_t*)len);
	else
		ptr = AMFReadInt32(ptr, end, (uint32_t*)len);

	if (NULL == ptr || ptr + len > end || len + 1 > length) return NULL;

	memcpy(string, ptr, len);
	string[len] = 0; // null-terminal string
	return ptr + len;
}

uint8_t* AMFWriteNamedString(uint8_t* ptr, const uint8_t* end, const char* name, size_t length, const char* value, size_t length2)
{
	if (ptr + length + 2 + length2 + 3 > end)
		return NULL;

	ptr = AMFWriteString16(ptr, end, name, length);

	return AMFWriteString(ptr, end, value, length2);
}

uint8_t* AMFWriteNamedDouble(uint8_t* ptr, const uint8_t* end, const char* name, size_t length, double value)
{
	if (ptr + length + 2 + 8 + 1 > end)
		return NULL;

	ptr = AMFWriteString16(ptr, end, name, length);

	return AMFWriteDouble(ptr, end, value);
}

uint8_t* AMFWriteNamedBoolean(uint8_t* ptr, const uint8_t* end, const char* name, size_t length, int value)
{
	if (ptr + length + 2 + 2 > end)
		return NULL;

	ptr = AMFWriteString16(ptr, end, name, length);

	return AMFWriteBoolean(ptr, end, value);
}
