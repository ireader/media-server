#include "flv-reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <memory.h>
#include "byte-order.h"

struct flv_reader_t
{
	FILE* fp;
	int header;
};

static int flv_reader_header(struct flv_reader_t* flv, void* buffer, size_t bytes)
{
	uint32_t offset;
	uint8_t flvheader[9];
	
	if (sizeof(flvheader) != fread(flvheader, 1, sizeof(flvheader), flv->fp))
		return -1;

	if ('F' != flvheader[0] || 'L' != flvheader[1] || 'V' != flvheader[2])
		return -1;

	assert(0x00 == (flvheader[4] & 0xF8) && 0x00 == (flvheader[4] & 0x20));
	be_read_uint32(flvheader+5, &offset);

	assert(offset >= sizeof(flvheader));
	if (bytes < offset + 4)
		return offset + 4;

	if (offset > sizeof(flvheader))
	{
		if (offset - sizeof(flvheader) != fread((uint8_t*)buffer + sizeof(flvheader), 1, offset - sizeof(flvheader), flv->fp))
			return -1;
	}

	// read with PreviousTagSize0
	if (offset + 4 - sizeof(flvheader) != fread((uint8_t*)buffer + sizeof(flvheader), 1, offset - sizeof(flvheader) + 4, flv->fp))
		return -1;

	memcpy(buffer, flvheader, sizeof(flvheader));
	return offset + 4;
}

void* flv_reader_create(const char* file)
{
	struct flv_reader_t* flv;
	flv = (struct flv_reader_t*)malloc(sizeof(*flv));
	if (NULL == flv)
		return NULL;

	flv->fp = fopen(file, "rb");
	if (NULL == flv->fp)
	{
		free(flv);
		return NULL;
	}

	flv->header = 0;
	return flv;
}

void flv_reader_destroy(void* p)
{
	struct flv_reader_t* flv;
	flv = (struct flv_reader_t*)p;

	if (flv->fp)
		fclose(flv->fp);
	
	free(flv);
}

int flv_reader_read(void* p, void* buffer, size_t bytes)
{
	uint8_t header[4];
	uint32_t tagbytes;
	struct flv_reader_t* flv;
	flv = (struct flv_reader_t*)p;

	// read flv header
	if (0 == flv->header)
	{
		flv->header = 1;
		return flv_reader_header(flv, buffer, bytes);
	}

	if (4 != fread(&header, 1, 4, flv->fp))
		return -1; // read file error

	tagbytes = (header[1] << 16) | (header[2] << 8) | header[3];
	if (bytes < tagbytes + 15)
		return tagbytes + 15;

	if (tagbytes + 11 != fread((uint8_t*)buffer + 4, 1, tagbytes + 11, flv->fp))
		return -1;

	memcpy(buffer, header, 4);
	return tagbytes + 15;
}
