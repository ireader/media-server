#include "amf0.h"
#include <stddef.h>
#include <string.h>
#include <assert.h>

static double s_double = 1.0; // 3ff0 0000 0000 0000

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
	*ptr++ = AMF_OBJECT_END;
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

	// Little-Endian
	if (0x00 == *(char*)&s_double)
	{
		*ptr++ = ((uint8_t*)&value)[7];
		*ptr++ = ((uint8_t*)&value)[6];
		*ptr++ = ((uint8_t*)&value)[5];
		*ptr++ = ((uint8_t*)&value)[4];
		*ptr++ = ((uint8_t*)&value)[3];
		*ptr++ = ((uint8_t*)&value)[2];
		*ptr++ = ((uint8_t*)&value)[1];
		*ptr++ = ((uint8_t*)&value)[0];
	}
	else
	{
		memcpy(ptr, &value, 8);
	}
	return ptr;
}

uint8_t* AMFWriteString(uint8_t* ptr, const uint8_t* end, const char* string, size_t length)
{
	if (!ptr || ptr + 1 + (length < 65536 ? 2 : 4) + length > end || length > UINT32_MAX)
		return NULL;

	if (length < 65536)
	{
		*ptr++ = AMF_STRING;
		AMFWriteString16(ptr, end, string, length);
		ptr += 2;
	}
	else
	{
		*ptr++ = AMF_LONG_STRING;
		AMFWriteString32(ptr, end, string, length);
		ptr += 4;
	}
	return ptr + length;
}

uint8_t* AMFWriteNamedBoolean(uint8_t* ptr, const uint8_t* end, const char* name, size_t length, uint8_t value)
{
	if (ptr + length + 2 + 2 > end)
		return NULL;

	ptr = AMFWriteString16(ptr, end, name, length);
	return ptr ? AMFWriteBoolean(ptr, end, value) : NULL;
}

uint8_t* AMFWriteNamedDouble(uint8_t* ptr, const uint8_t* end, const char* name, size_t length, double value)
{
	if (ptr + length + 2 + 8 + 1 > end)
		return NULL;

	ptr = AMFWriteString16(ptr, end, name, length);
	return ptr ? AMFWriteDouble(ptr, end, value) : NULL;
}

uint8_t* AMFWriteNamedString(uint8_t* ptr, const uint8_t* end, const char* name, size_t length, const char* value, size_t length2)
{
	if (ptr + length + 2 + length2 + 3 > end)
		return NULL;

	ptr = AMFWriteString16(ptr, end, name, length);
	return ptr ? AMFWriteString(ptr, end, value, length2) : NULL;
}

static const uint8_t* AMFReadInt16(const uint8_t* ptr, const uint8_t* end, uint32_t* value)
{
	if (!ptr || end - ptr < 2)
		return NULL;

	if (value)
	{
		*value = ((uint32_t)ptr[0] << 8) | ptr[1];
	}
	return ptr + 2;
}

static const uint8_t* AMFReadInt32(const uint8_t* ptr, const uint8_t* end, uint32_t* value)
{
	if (!ptr || end - ptr < 4)
		return NULL;

	if (value)
	{
		*value = ((uint32_t)ptr[0] << 24) | ((uint32_t)ptr[1] << 16) | ((uint32_t)ptr[2] << 8) | ptr[3];
	}
	return ptr + 4;
}

const uint8_t* AMFReadNull(const uint8_t* ptr, const uint8_t* end)
{
	assert(ptr && end);
	return ptr;
}

const uint8_t* AMFReadBoolean(const uint8_t* ptr, const uint8_t* end, uint8_t* value)
{
	if (!ptr || end - ptr < 1)
		return NULL;

	if (value)
	{
		*value = ptr[0];
	}
	return ptr + 1;
}

const uint8_t* AMFReadDouble(const uint8_t* ptr, const uint8_t* end, double* value)
{
	uint8_t* p = (uint8_t*)value;
	if (!ptr || end - ptr < 8)
		return NULL;

	if (value)
	{
		if (0x00 == *(char*)&s_double)
		{// Little-Endian
			*p++ = ptr[7];
			*p++ = ptr[6];
			*p++ = ptr[5];
			*p++ = ptr[4];
			*p++ = ptr[3];
			*p++ = ptr[2];
			*p++ = ptr[1];
			*p++ = ptr[0];
		}
		else
		{
			memcpy(&value, ptr, 8);
		}
	}
	return ptr + 8;
}

const uint8_t* AMFReadString(const uint8_t* ptr, const uint8_t* end, int isLongString, char* string, size_t length)
{ 
	uint32_t len = 0;
	if (0 == isLongString)
		ptr = AMFReadInt16(ptr, end, &len);
	else
		ptr = AMFReadInt32(ptr, end, &len);

	if (!ptr || ptr + len > end)
		return NULL;

	if (string && length > len)
	{
		memcpy(string, ptr, len);
		string[len] = 0;
	}
	return ptr + len;
}


static const uint8_t* amf_read_object(const uint8_t* data, const uint8_t* end, struct amf_object_item_t* items, size_t n);
static const uint8_t* amf_read_ecma_array(const uint8_t* data, const uint8_t* end);

static const uint8_t* amf_read_item(const uint8_t* data, const uint8_t* end, enum AMFDataType type, struct amf_object_item_t* item)
{
	switch (type)
	{
	case AMF_BOOLEAN:
		return AMFReadBoolean(data, end, (uint8_t*)(item ? item->value : NULL));

	case AMF_NUMBER:
		return AMFReadDouble(data, end, (double*)(item ? item->value : NULL));

	case AMF_STRING:
		return AMFReadString(data, end, 0, (char*)(item ? item->value : NULL), item ? item->size : 0);

	case AMF_LONG_STRING:
		return AMFReadString(data, end, 1, (char*)(item ? item->value : NULL), item ? item->size : 0);

	case AMF_OBJECT:
		return amf_read_object(data, end, (struct amf_object_item_t*)(item ? item->value : NULL), item ? item->size : 0);

	case AMF_NULL:
		return data;

	case AMF_ECMA_ARRAY:
		return amf_read_ecma_array(data, end);

	default:
		assert(0);
		return NULL;
	}
}

static const uint8_t* amf_read_ecma_array(const uint8_t* ptr, const uint8_t* end)
{
	if (!ptr || ptr + 4 > end)
		return NULL;
	ptr += 4; // U32 associative-count
	return amf_read_object(ptr, end, NULL, 0);
}

static const uint8_t* amf_read_object(const uint8_t* data, const uint8_t* end, struct amf_object_item_t* items, size_t n)
{
	uint8_t type;
	uint32_t len;
	size_t i;

	while (data && data + 2 <= end)
	{
		len = *data++ << 8;
		len |= *data++;
		if (0 == len)
			break; // last item

		if (data + len + 1 > end)
			return NULL; // invalid

		for (i = 0; i < n; i++)
		{
			if (0 == memcmp(items[i].name, data, len) && strlen(items[i].name) == len && data[len] == items[i].type)
				break;
		}

		data += len; // skip name string
		type = *data++; // value type
		data = amf_read_item(data, end, type, i < n ? &items[i] : NULL);
	}

	if (data && data < end && AMF_OBJECT_END == *data)
		return data + 1;
	return NULL; // invalid object
}

const uint8_t* amf_read_items(const uint8_t* data, const uint8_t* end, struct amf_object_item_t* items, size_t count)
{
	size_t i;
	uint8_t type;
	for (i = 0; i < count && data && data < end; i++)
	{
		type = *data++;
		if (type != items[i].type && !(AMF_OBJECT == items[i].type && AMF_NULL == type))
			return NULL;

		data = amf_read_item(data, end, type, &items[i]);
	}

	return data;
}
