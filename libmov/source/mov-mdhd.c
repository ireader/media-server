#include "mov-mdhd.h"
#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <assert.h>

// 8.4.2 Media Header Box (p35)
// Box Type: ¡®mdhd¡¯ 
// Container: Media Box (¡®mdia¡¯) 
// Mandatory: Yes 
// Quantity: Exactly one

/*
aligned(8) class MediaHeaderBox extends FullBox(¡®mdhd¡¯, version, 0) { 
	if (version==1) { 
		unsigned int(64) creation_time; 
		unsigned int(64) modification_time; 
		unsigned int(32) timescale; 
		unsigned int(64) duration; 
	} else { // version==0 
		unsigned int(32) creation_time; 
		unsigned int(32) modification_time; 
		unsigned int(32) timescale; 
		unsigned int(32) duration; 
	} 
	bit(1) pad = 0; 
	unsigned int(5)[3] language; // ISO-639-2/T language code 
	unsigned int(16) pre_defined = 0; 
}
*/
int mov_read_mdhd(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t val;
	struct mov_mdhd_t* mdhd = &mov->track->mdhd;

	mdhd->version = file_reader_r8(mov->fp);
	mdhd->flags = file_reader_rb24(mov->fp);

	if (1 == mdhd->version)
	{
		mdhd->creation_time = file_reader_rb64(mov->fp);
		mdhd->modification_time = file_reader_rb64(mov->fp);
		mdhd->timescale = file_reader_rb32(mov->fp);
		mdhd->duration = file_reader_rb64(mov->fp);
	}
	else
	{
		assert(0 == mdhd->version);
		mdhd->creation_time = file_reader_rb32(mov->fp);
		mdhd->modification_time = file_reader_rb32(mov->fp);
		mdhd->timescale = file_reader_rb32(mov->fp);
		mdhd->duration = file_reader_rb32(mov->fp);
	}

	val = file_reader_rb32(mov->fp);
	mdhd->language = (val >> 16) & 0x7FFF;
	mdhd->pre_defined = val & 0xFFFF;
	return 0;
}

size_t mov_write_mdhd(const struct mov_t* mov)
{
	const struct mov_track_t* track = mov->track;
	
	file_writer_wb32(mov->fp, 32); /* size */
	file_writer_write(mov->fp, "mdhd", 4);
	file_writer_wb32(mov->fp, 0); /* version 1 & flags */

	file_writer_wb32(mov->fp, (uint32_t)track->mdhd.creation_time); /* creation_time */
	file_writer_wb32(mov->fp, (uint32_t)track->mdhd.modification_time); /* modification_time */
	file_writer_wb32(mov->fp, track->mdhd.timescale); /* timescale */
	file_writer_wb32(mov->fp, (uint32_t)track->mdhd.duration); /* duration */
	
	file_writer_wb16(mov->fp, (uint16_t)track->mdhd.language); /* ISO-639-2/T language code */
	file_writer_wb16(mov->fp, 0); /* pre_defined (quality) */
	return 32;
}
