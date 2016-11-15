#include "mov-stsd.h"
#include "file-reader.h"
#include "mov-internal.h"
#include <assert.h>

static int mov_read_sample_entry(struct mov_reader_t* mov, struct mov_box_t* box)
{
	box->size = file_reader_rb32(mov->fp);
	box->type = file_reader_rb32(mov->fp);
	file_reader_seek(mov->fp, 6); // const unsigned int(8)[6] reserved = 0;
	uint16_t data_reference_index = file_reader_rb16(mov->fp);
	return 0;
}

static int mov_read_audio(struct mov_reader_t* mov, const struct mov_box_t* stsd)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box);
#if 0
	// const unsigned int(32)[2] reserved = 0;
	file_reader_seek(mov->fp, 8);
#else
	file_reader_rb16(mov->fp); /* version */
	file_reader_rb16(mov->fp); /* revision level */
	file_reader_rb32(mov->fp); /* vendor */
#endif
	uint16_t channelcount = file_reader_rb16(mov->fp);
	uint16_t samplesize = file_reader_rb16(mov->fp);
#if 0
	// unsigned int(16) pre_defined = 0; 
	// const unsigned int(16) reserved = 0 ;
	file_reader_seek(mov->fp, 4);
#else
	file_reader_rb16(mov->fp); /* audio cid */
	file_reader_rb16(mov->fp); /* packet size = 0 */
#endif

	uint32_t samplerate = file_reader_rb32(mov->fp); // { default samplerate of media}<<16;
	file_reader_seek(mov->fp, box.size - 36);
	return 0;
}

static int mov_read_video(struct mov_reader_t* mov, const struct mov_box_t* stsd)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box);
#if 0
	 //unsigned int(16) pre_defined = 0; 
	 //const unsigned int(16) reserved = 0;
	 //unsigned int(32)[3] pre_defined = 0;
	file_reader_seek(mov->fp, 16);
#else
	file_reader_rb16(mov->fp); /* version */
	file_reader_rb16(mov->fp); /* revision level */
	file_reader_rb32(mov->fp); /* vendor */
	file_reader_rb32(mov->fp); /* temporal quality */
	file_reader_rb32(mov->fp); /* spatial quality */
#endif
	uint16_t width = file_reader_rb16(mov->fp);
	uint16_t height = file_reader_rb16(mov->fp);
	uint32_t horizresolution = file_reader_rb32(mov->fp); // 0x00480000 - 72 dpi
	uint32_t vertresolution = file_reader_rb32(mov->fp); // 0x00480000 - 72 dpi
	// const unsigned int(32) reserved = 0;
	file_reader_rb32(mov->fp); /* data size, always 0 */
	uint16_t frame_count = file_reader_rb16(mov->fp);

	//string[32] compressorname;
	//uint32_t len = file_reader_r8(mov->fp);
	//file_reader_seek(mov->fp, len);
	file_reader_seek(mov->fp, 32);

	uint16_t depth = file_reader_rb16(mov->fp);
	// int(16) pre_defined = -1;
	file_reader_seek(mov->fp, 2);
	file_reader_seek(mov->fp, box.size - 86);
	return 0;
}

static int mov_read_hint_sample_entry(struct mov_reader_t* mov, const struct mov_box_t* stsd)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box);
	return file_reader_seek(mov->fp, box.size - 16);
}

static int mov_read_meta_sample_entry(struct mov_reader_t* mov, const struct mov_box_t* stsd)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box);
	return file_reader_seek(mov->fp, box.size - 16);
}

int mov_read_stsd(struct mov_reader_t* mov, const struct mov_box_t* box)
{
	int i;
	uint64_t pos, pos2;
	struct mov_stsd_t stsd;

	stsd.version = file_reader_r8(mov->fp);
	stsd.flags = file_reader_rb24(mov->fp);
	stsd.entry_count = file_reader_rb32(mov->fp);

	for (i = 0; i < stsd.entry_count; i++)
	{
		if (MOV_AUDIO == mov->handler_type)
		{
			mov_read_audio(mov, box);
		}
		else if (MOV_VIDEO == mov->handler_type)
		{
			mov_read_video(mov, box);
		}
		else if (MOV_TAG('h', 'i', 'n', 't') == mov->handler_type)
		{
			mov_read_hint_sample_entry(mov, box);
		}
		else if (MOV_TAG('m', 'e', 't', 'a') == mov->handler_type)
		{
			mov_read_meta_sample_entry(mov, box);
		}
		else
		{
			assert(0);
		}
	}

	return 0;
}
