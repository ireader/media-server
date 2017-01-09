#include "mov-stsd.h"
#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <assert.h>

// stsd: Sample Description Box
#define MOV_AUDIO MOV_TAG('s', 'o', 'u', 'n')
#define MOV_VIDEO MOV_TAG('v', 'i', 'd', 'e')
#define MOV_HINT  MOV_TAG('h', 'i', 'n', 't')
#define MOV_META  MOV_TAG('m', 'e', 't', 'a')

static int mov_read_sample_entry(struct mov_t* mov, struct mov_box_t* box)
{
	box->size = file_reader_rb32(mov->fp);
	box->type = file_reader_rb32(mov->fp);
	file_reader_seek(mov->fp, 6); // const unsigned int(8)[6] reserved = 0;
	uint16_t data_reference_index = file_reader_rb16(mov->fp);
	return 0;
}

static int mov_read_audio(struct mov_t* mov, const struct mov_box_t* stsd)
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

static int mov_read_video(struct mov_t* mov, const struct mov_box_t* stsd)
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

static int mov_read_hint_sample_entry(struct mov_t* mov, const struct mov_box_t* stsd)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box);
	return file_reader_seek(mov->fp, box.size - 16);
}

static int mov_read_meta_sample_entry(struct mov_t* mov, const struct mov_box_t* stsd)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box);
	return file_reader_seek(mov->fp, box.size - 16);
}

int mov_read_stsd(struct mov_t* mov, const struct mov_box_t* box)
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
		else if (MOV_HINT == mov->handler_type)
		{
			mov_read_hint_sample_entry(mov, box);
		}
		else if (MOV_META == mov->handler_type)
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

//static int mov_write_h264(const struct mov_t* mov)
//{
//	size_t size;
//	uint64_t offset;
//	const struct mov_track_t* track = mov->track;
//
//	size = 8 /* Box */;
//
//	offset = file_writer_tell(mov->fp);
//	file_writer_wb32(mov->fp, 0); /* size */
//	file_writer_wb32(mov->fp, MOV_TAG('a', 'v', 'c', 'C'));
//
//	mov_write_size(mov->fp, offset, size); /* update size */
//	return size;
//}

static int mov_write_video(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;
	const struct mov_track_t* track = mov->track;

	size = 8 /* Box */ + 8 /* SampleEntry */ + 70 /* VisualSampleEntry */;

	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "h264", 4);

	file_writer_wb32(mov->fp, 0); /* Reserved */
	file_writer_wb16(mov->fp, 0); /* Reserved */
	file_writer_wb16(mov->fp, 1); /* Data-reference index */

	file_writer_wb16(mov->fp, 0); /* Reserved / Codec stream version */
	file_writer_wb16(mov->fp, 0); /* Reserved / Codec stream revision (=0) */
	file_writer_wb32(mov->fp, 0); /* Reserved */
	file_writer_wb32(mov->fp, 0); /* Reserved */
	file_writer_wb32(mov->fp, 0); /* Reserved */

	file_writer_wb16(mov->fp, track->av.video.width); /* Video width */
	file_writer_wb16(mov->fp, track->av.video.height); /* Video height */
	file_writer_wb32(mov->fp, 0x00480000); /* Horizontal resolution 72dpi */
	file_writer_wb32(mov->fp, 0x00480000); /* Vertical resolution 72dpi */
	file_writer_wb32(mov->fp, 0); /* reserved / Data size (= 0) */
	file_writer_wb16(mov->fp, 1); /* Frame count (= 1) */

	file_writer_w8(mov->fp, 0 /*strlen(compressor_name)*/); /* compressorname */
	file_writer_write(mov->fp, " ", 31); // fill empty

	file_writer_wb16(mov->fp, 0x18); /* Reserved */
	file_writer_wb16(mov->fp, 0xffff); /* Reserved */

	//size += mov_write_h264(mov);
	//size += mov_write_mp4v(mov);

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

static int mov_write_audio(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;
	const struct mov_track_t* track = mov->track;

	size = 8 /* Box */ + 8 /* SampleEntry */ + 20 /* AudioSampleEntry */;

	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "aac ", 4);

	file_writer_wb32(mov->fp, 0); /* Reserved */
	file_writer_wb16(mov->fp, 0); /* Reserved */
	file_writer_wb16(mov->fp, 1); /* Data-reference index */

	/* SoundDescription */
	file_writer_wb16(mov->fp, 0); /* Version */
	file_writer_wb16(mov->fp, 0); /* Revision level */
	file_writer_wb32(mov->fp, 0); /* Reserved */

	file_writer_wb16(mov->fp, 2); /* channelcount */
	file_writer_wb16(mov->fp, 16); /* samplesize */

	file_writer_wb16(mov->fp, 0); /* pre_defined */
	file_writer_wb16(mov->fp, 0); /* reserved / packet size (= 0) */

	file_writer_wb16(mov->fp, track->av.audio.sample_rate);
	file_writer_wb16(mov->fp, 0); /* Reserved */

	//size += mov_write_mp4a(mov);

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}

size_t mov_write_stsd(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;
	const struct mov_track_t* track = mov->track;

	size = 12 /* full box */ + 4 /* entry count */;

	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "stsd", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */
	file_writer_wb32(mov->fp, 1); /* entry count */

	if(track->stream_type == AVSTREAM_VIDEO)
	{
		size += mov_write_video(mov);
	}
	else if(track->codec_id == AVSTREAM_AUDIO)
	{
		size += mov_write_audio(mov);
	}
	else
	{
		assert(0);
	}

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}
