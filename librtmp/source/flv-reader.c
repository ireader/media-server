#include "flv-reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef uint8_t bool_t;

struct flv_header_t
{
	//uint8_t F;
	//uint8_t L;
	//uint8_t V;
	uint8_t version;
	bool_t audio;
	bool_t video;
	uint32_t dataoffset;
};

struct flv_tag_t
{
	bool_t filter; // 0-No pre-processing required
	uint8_t type; // 8-audio, 9-video, 18-script data
	uint32_t datasize;
	uint32_t timestamp;
	uint32_t streamId;
};

static uint32_t be_read_uint32(const uint8_t* ptr)
{
	return (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
}

static int flv_read_header(FILE* fp)
{
	uint8_t data[9];
	uint32_t offset;

	if (sizeof(data) != fread(data, 1, sizeof(data), fp))
		return -1;

	if ('F' != data[0] || 'L' != data[1] || 'V' != data[2])
		return -1;

	assert(0x00 == (data[4] & 0xF8) && 0x00 == (data[4] & 0x20));
	offset = be_read_uint32(data + 5);

	assert(offset >= sizeof(data));
	if(offset > sizeof(data))
		fseek(fp, offset, SEEK_SET); // skip

	// PreviousTagSize0
	if (4 != fread(data, 1, 4, fp))
		return -1;

	assert(be_read_uint32(data) == 0);
	return be_read_uint32(data) == 0 ? 0 : -1;
}

void* flv_reader_create(const char* file)
{
	FILE* fp;
	fp = fopen(file, "rb");
	if (NULL == fp || 0 != flv_read_header(fp))
	{
		flv_reader_destroy(fp);
		return NULL;
	}

	return fp;
}

void flv_reader_destroy(void* p)
{
	FILE* fp = (FILE*)p;
	if (NULL != fp)
		fclose(fp);
}

int flv_reader_read(void* p, int* tagtype, uint32_t* timestamp, void* buffer, size_t bytes)
{
	uint8_t header[11];
	uint32_t datasize;
	FILE* fp = (FILE*)p;

	if (11 != fread(&header, 1, 11, fp))
		return -1; // read file error

	// DataSize
	datasize = (header[1] << 16) | (header[2] << 8) | header[3];
	if (bytes < datasize)
		return datasize;

	// TagType
	*tagtype = header[0] & 0x1F;

	// TimestampExtended | Timestamp
	*timestamp = (header[4] << 16) | (header[5] << 8) | header[6] | (header[7] << 24);

	// StreamID Always 0
	assert(0 == header[8] && 0 == header[9] && 0 == header[10]);

	// FLV stream
	if(datasize != fread(buffer, 1, datasize, fp))
		return -1;

	// PreviousTagSizeN
	if (4 != fread(header, 1, 4, fp))
		return -1;

	assert(be_read_uint32(header) == datasize + 11);
	return (be_read_uint32(header) == datasize + 11) ? datasize : -1;
}
