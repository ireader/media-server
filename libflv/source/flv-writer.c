#include "flv-writer.h"
#include "flv-header.h"
#include "flv-proto.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define FLV_HEADER_SIZE		9 // DataOffset included
#define FLV_TAG_HEADER_SIZE	11 // StreamID included

struct flv_writer_t
{
	FILE* fp;
	int (*write)(void* param, const void* buf, int len);
	void* param;
};

static int flv_write_header(struct flv_writer_t* flv)
{
	uint8_t header[FLV_HEADER_SIZE + 4];
	flv_header_write(1, 1, header, FLV_HEADER_SIZE);
    flv_tag_size_write(header + FLV_HEADER_SIZE, 4, 0); // PreviousTagSize0(Always 0)
	return sizeof(header) == flv->write(flv->param, header, sizeof(header)) ? 0 : -1;
}

static int flv_write_eos(struct flv_writer_t* flv)
{
	int n;
	uint8_t header[16];
	struct flv_video_tag_header_t video;
	memset(&video, 0, sizeof(video));
	video.codecid = FLV_VIDEO_H264;
	video.keyframe = 1;
	video.avpacket = FLV_END_OF_SEQUENCE;
	video.cts = 0;

	n = flv_video_tag_header_write(&video, header, sizeof(header));
	return n > 0 ? flv_writer_input(flv, FLV_TYPE_VIDEO, header, n, 0) : -1;
}

static int file_write(void* param, const void* buf, int len)
{
	return (int)fwrite(buf, 1, len, (FILE*)param);
}

void* flv_writer_create(const char* file)
{
	FILE* fp;
	struct flv_writer_t* flv;
	fp = fopen(file, "wb");
	if (!fp)
		return NULL;

	flv = flv_writer_create2(file_write, fp);
	if (!flv)
	{
		fclose(fp);
		return NULL;
	}

	flv->fp = fp;
	return flv;
}

void* flv_writer_create2(int (*write)(void* param, const void* buf, int len), void* param)
{
	struct flv_writer_t* flv;
	flv = (struct flv_writer_t*)calloc(1, sizeof(*flv));
	if (!flv)
		return NULL;

	flv->write = write;
	flv->param = param;
	if (0 != flv_write_header(flv))
	{
		flv_writer_destroy(flv);
		return NULL;
	}

	return flv;
}

void flv_writer_destroy(void* p)
{
	struct flv_writer_t* flv;
	flv = (struct flv_writer_t*)p;

	if (NULL != flv)
	{
		flv_write_eos(flv);
		if (flv->fp)
			fclose(flv->fp);
		free(flv);
	}
}

int flv_writer_input(void* p, int type, const void* data, size_t bytes, uint32_t timestamp)
{
	uint8_t buf[FLV_TAG_HEADER_SIZE + 4];
	struct flv_writer_t* flv;
	struct flv_tag_header_t tag;
	flv = (struct flv_writer_t*)p;

	memset(&tag, 0, sizeof(tag));
	tag.size = (int)bytes;
	tag.type = (uint8_t)type;
	tag.timestamp = timestamp;
	flv_tag_header_write(&tag, buf, FLV_TAG_HEADER_SIZE);
	flv_tag_size_write(buf + FLV_TAG_HEADER_SIZE, 4, (uint32_t)bytes + FLV_TAG_HEADER_SIZE);

	if(FLV_TAG_HEADER_SIZE != flv->write(flv->param, buf, FLV_TAG_HEADER_SIZE) // FLV Tag Header
		|| bytes != (size_t)flv->write(flv->param, data, (int)bytes)
		|| 4 != flv->write(flv->param, buf + FLV_TAG_HEADER_SIZE, 4)) // TAG size
		return -1;
	return 0;
}
