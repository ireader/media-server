#include "flv-writer.h"
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#define FLV_TYPE_AUDIO 8
#define FLV_TYPE_VDIEO 9
#define FLV_TYPE_SCRIPT 18

static void be_write_uint32(uint8_t* ptr, uint32_t val)
{
	ptr[0] = (uint8_t)((val >> 24) & 0xFF);
	ptr[1] = (uint8_t)((val >> 16) & 0xFF);
	ptr[2] = (uint8_t)((val >> 8) & 0xFF);
	ptr[3] = (uint8_t)(val & 0xFF);
}

static int flv_write_tag(uint8_t* tag, uint8_t type, uint32_t bytes, uint32_t timestamp)
{
	// TagType
	tag[0] = type & 0x1F;

	// DataSize
	tag[1] = (bytes >> 16) & 0xFF;
	tag[2] = (bytes >> 8) & 0xFF;
	tag[3] = bytes & 0xFF;

	// Timestamp
	tag[4] = (timestamp >> 16) & 0xFF;
	tag[5] = (timestamp >> 8) & 0xFF;
	tag[6] = (timestamp >> 0) & 0xFF;
	tag[7] = (timestamp >> 24) & 0xFF; // Timestamp Extended

	// StreamID(Always 0)
	tag[8] = 0;
	tag[9] = 0;
	tag[10] = 0;

	return 11;
}

static int flv_write_header(FILE* fp)
{
	uint8_t header[9+4];
	header[0] = 'F'; // FLV signature
	header[1] = 'L';
	header[2] = 'V';
	header[3] = 0x01; // File version
	header[4] = 0x05; // Type flags (audio & video)
	be_write_uint32(header + 5, 9); // Data offset
	be_write_uint32(header + 9, 0); // PreviousTagSize0(Always 0)

	if (sizeof(header) != fwrite(header, 1, sizeof(header), fp))
		return ferror(fp);
	return 0;
}

static int flv_write_eos(FILE* fp)
{
	uint8_t header[11 + 5 + 4];
	flv_write_tag(header, FLV_TYPE_VDIEO, 5, 0);
	header[11] = (1 << 4) /* FrameType */ | 7 /* AVC */;
	header[12] = 2; // AVC end of sequence
	header[13] = 0;
	header[14] = 0;
	header[15] = 0;
	be_write_uint32(header + 16, 16); // TAG size

	if (sizeof(header) != fwrite(header, 1, sizeof(header), fp))
		return ferror(fp);
	return 0;
}

void* flv_writer_create(const char* file)
{
	FILE* fp;
	fp = fopen(file, "wb");
	if (!fp || 0 != flv_write_header(fp))
	{
		flv_writer_destroy(fp);
		return NULL;
	}

	return fp;
}

void flv_writer_destroy(void* p)
{
	FILE* fp = (FILE*)p;

	if (NULL != fp)
	{
		flv_write_eos(fp);
		fclose(fp);
	}
}

int flv_writer_input(void* p, int type, const void* data, size_t bytes, uint32_t timestamp)
{
	uint8_t tag[11];
	FILE* fp = (FILE*)p;

	flv_write_tag(tag, (uint8_t)type, (uint32_t)bytes, timestamp);
	fwrite(tag, 11, 1, fp); // FLV Tag Header

	fwrite(data, bytes, 1, fp);

	be_write_uint32(tag, (uint32_t)bytes + 11);
	fwrite(tag, 4, 1, fp); // TAG size

	return ferror(fp);
}
