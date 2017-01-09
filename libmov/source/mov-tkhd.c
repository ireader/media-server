#include "mov-tkhd.h"
#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <assert.h>

// ISO/IEC 14496-12:2012(E)
// 8.3.2 Track Header Box (p31)
// Box Type : ¡®tkhd¡¯ 
// Container : Movie Box(¡®trak¡¯) 
// Mandatory : Yes 
// Quantity : Exactly one

/*
aligned(8) class TrackHeaderBox extends FullBox(¡®tkhd¡¯, version, flags){ 
	if (version==1) { 
		unsigned int(64) creation_time; 
		unsigned int(64) modification_time; 
		unsigned int(32) track_ID; 
		const unsigned int(32) reserved = 0; 
		unsigned int(64) duration; 
	} else { // version==0 
		unsigned int(32) creation_time; 
		unsigned int(32) modification_time;
		unsigned int(32) track_ID; 
		const unsigned int(32) reserved = 0; 
		unsigned int(32) duration; 
	} 
	const unsigned int(32)[2] reserved = 0; 
	template int(16) layer = 0; 
	template int(16) alternate_group = 0; 
	template int(16) volume = {if track_is_audio 0x0100 else 0}; 
	const unsigned int(16) reserved = 0; 
	template int(32)[9] matrix= { 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 }; // unity matrix 
	unsigned int(32) width; 
	unsigned int(32) height; 
}
*/
int mov_read_tkhd(struct mov_t* mov, const struct mov_box_t* box)
{
	int i;
	struct mov_track_t* track = mov->track;

	track->tkhd.version = file_reader_r8(mov->fp);
	track->tkhd.flags = file_reader_rb24(mov->fp);

	if (1 == track->tkhd.version)
	{
		track->tkhd.creation_time = file_reader_rb64(mov->fp);
		track->tkhd.modification_time = file_reader_rb64(mov->fp);
		track->tkhd.track_ID = file_reader_rb32(mov->fp);
		/*track->tkhd.reserved = */file_reader_rb32(mov->fp);
		track->tkhd.duration = file_reader_rb64(mov->fp);
	}
	else
	{
		assert(0 == track->tkhd.version);
		track->tkhd.creation_time = file_reader_rb32(mov->fp);
		track->tkhd.modification_time = file_reader_rb32(mov->fp);
		track->tkhd.track_ID = file_reader_rb32(mov->fp);
		/*track->tkhd.reserved = */file_reader_rb32(mov->fp);
		track->tkhd.duration = file_reader_rb32(mov->fp);
	}

	// const unsigned int(32)[2] reserved = 0;
	file_reader_seek(mov->fp, 8);
	track->tkhd.layer = (uint16_t)file_reader_rb16(mov->fp);
	track->tkhd.alternate_group = (uint16_t)file_reader_rb16(mov->fp);
	track->tkhd.volume = (uint16_t)file_reader_rb16(mov->fp);
	// const unsigned int(16) reserved = 0;
	file_reader_seek(mov->fp, 2);
	for (i = 0; i < 9; i++)
		track->tkhd.matrix[i] = file_reader_rb32(mov->fp);
	track->tkhd.width = file_reader_rb32(mov->fp);
	track->tkhd.height = file_reader_rb32(mov->fp);
	return 0;
}

size_t mov_write_tkhd(const struct mov_t* mov)
{
//	int rotation = 0; // 90/180/270
	uint16_t group = 0;
	const struct mov_track_t* track = mov->track;

	file_writer_wb32(mov->fp, 92); /* size */
	file_writer_write(mov->fp, "tkhd", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */

	file_writer_wb32(mov->fp, (uint32_t)track->tkhd.creation_time); /* creation_time */
	file_writer_wb32(mov->fp, (uint32_t)track->tkhd.modification_time); /* modification_time */
	file_writer_wb32(mov->fp, track->tkhd.track_ID); /* timescale */
	file_writer_wb32(mov->fp, 0); /* reserved */
	file_writer_wb32(mov->fp, (uint32_t)track->tkhd.duration); /* duration */

	file_writer_wb32(mov->fp, 0); /* reserved */
	file_writer_wb32(mov->fp, 0); /* reserved */
	file_writer_wb16(mov->fp, 0); /* layer */
	file_writer_wb16(mov->fp, group); /* alternate_group */
	file_writer_wb16(mov->fp, AVSTREAM_AUDIO == track->stream_type ? 0x0100 : 0); /* volume */
	file_writer_wb16(mov->fp, 0); /* reserved */

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
	
	if(AVSTREAM_VIDEO == track->stream_type)
	{
		file_writer_wb32(mov->fp, track->av.video.width * 0x10000U); /* width */
		file_writer_wb32(mov->fp, track->av.video.height * 0x10000U); /* height */
	}
	else
	{
		file_writer_wb32(mov->fp, 0);
		file_writer_wb32(mov->fp, 0);
	}

	return 92;
}
