#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <assert.h>

// ISO/IEC 14496-12:2012(E)
// 8.2.2.1 Movie Header Box (p30)
// Box Type : ¡®mvhd¡¯ 
// Container : Movie Box(¡®moov¡¯) 
// Mandatory : Yes 
// Quantity : Exactly one

/*
aligned(8) class MovieHeaderBox extends FullBox(¡®mvhd¡¯, version, 0) { 
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
	template int(32) rate = 0x00010000; // typically 1.0 
	template int(16) volume = 0x0100; // typically, full volume 
	const bit(16) reserved = 0; 
	const unsigned int(32)[2] reserved = 0; 
	template int(32)[9] matrix = { 
		0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 
	}; // Unity matrix 
	bit(32)[6] pre_defined = 0; 
	unsigned int(32) next_track_ID; 
}
*/
int mov_read_mvhd(struct mov_t* mov, const struct mov_box_t* box)
{
	int i;
	struct mov_mvhd_t mvhd;

	mvhd.version = file_reader_r8(mov->fp);
	mvhd.flags = file_reader_rb24(mov->fp);

	if (1 == mvhd.version)
	{
		mvhd.creation_time = file_reader_rb64(mov->fp);
		mvhd.modification_time = file_reader_rb64(mov->fp);
		mvhd.timescale = file_reader_rb32(mov->fp);
		mvhd.duration = file_reader_rb64(mov->fp);
	}
	else
	{
		assert(0 == mvhd.version);
		mvhd.creation_time = file_reader_rb32(mov->fp);
		mvhd.modification_time = file_reader_rb32(mov->fp);
		mvhd.timescale = file_reader_rb32(mov->fp);
		mvhd.duration = file_reader_rb32(mov->fp);
	}

	mvhd.rate = file_reader_rb32(mov->fp);
	mvhd.volume = (uint16_t)file_reader_rb16(mov->fp);
	//mvhd.reserved = file_reader_rb16(mov->fp);
	//mvhd.reserved2[0] = file_reader_rb32(mov->fp);
	//mvhd.reserved2[1] = file_reader_rb32(mov->fp);
	file_reader_seek(mov->fp, 10);
	for (i = 0; i < 9; i++)
		mvhd.matrix[i] = file_reader_rb32(mov->fp);
#if 0
	for (i = 0; i < 6; i++)
		mvhd.pre_defined[i] = file_reader_rb32(mov->fp);
#else
	file_reader_rb32(mov->fp); /* preview time */
	file_reader_rb32(mov->fp); /* preview duration */
	file_reader_rb32(mov->fp); /* poster time */
	file_reader_rb32(mov->fp); /* selection time */
	file_reader_rb32(mov->fp); /* selection duration */
	file_reader_rb32(mov->fp); /* current time */
#endif
	mvhd.next_track_ID = file_reader_rb32(mov->fp);
	return 0;
}

size_t mov_write_mvhd(const struct mov_t* mov)
{
//	int rotation = 0; // 90/180/270
	const struct mov_track_t* track = mov->track;

	file_writer_wb32(mov->fp, 108); /* size */
	file_writer_write(mov->fp, "mvhd", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */

	file_writer_wb32(mov->fp, (uint32_t)track->mvhd.creation_time); /* creation_time */
	file_writer_wb32(mov->fp, (uint32_t)track->mvhd.modification_time); /* modification_time */
	file_writer_wb32(mov->fp, track->mvhd.timescale); /* timescale */
	file_writer_wb32(mov->fp, (uint32_t)track->mvhd.duration); /* duration */

	file_writer_wb32(mov->fp, 0x00010000); /* rate 1.0 */
	file_writer_wb16(mov->fp, 0x0100); /* volume 1.0 = normal */
	file_writer_wb16(mov->fp, 0); /* reserved */
	file_writer_wb32(mov->fp, 0); /* reserved */
	file_writer_wb32(mov->fp, 0); /* reserved */

	// matrix
	file_writer_wb32(mov->fp, 0x00010000); /* u */
	file_writer_wb32(mov->fp, 0);
	file_writer_wb32(mov->fp, 0);
	file_writer_wb32(mov->fp, 0); /* v */
	file_writer_wb32(mov->fp, 0x00010000);
	file_writer_wb32(mov->fp, 0);
	file_writer_wb32(mov->fp, 0); /* w */
	file_writer_wb32(mov->fp, 0);
	file_writer_wb32(mov->fp, 0x40000000);

	file_writer_wb32(mov->fp, 0); /* reserved (preview time) */
	file_writer_wb32(mov->fp, 0); /* reserved (preview duration) */
	file_writer_wb32(mov->fp, 0); /* reserved (poster time) */
	file_writer_wb32(mov->fp, 0); /* reserved (selection time) */
	file_writer_wb32(mov->fp, 0); /* reserved (selection duration) */
	file_writer_wb32(mov->fp, 0); /* reserved (current time) */

	file_writer_wb32(mov->fp, track->mvhd.next_track_ID); /* Next track id */

	return 108;
}
