#include "mov-tkhd.h"
#include "file-reader.h"
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
int mov_read_tkhd(struct mov_reader_t* mov, const struct mov_box_t* box)
{
	int i;
	struct mov_tkhd_t tkhd;

	tkhd.version = file_reader_r8(mov->fp);
	tkhd.flags = file_reader_rb24(mov->fp);

	if (1 == tkhd.version)
	{
		tkhd.v1.creation_time = file_reader_rb64(mov->fp);
		tkhd.v1.modification_time = file_reader_rb64(mov->fp);
		tkhd.v1.track_ID = file_reader_rb32(mov->fp);
		/*tkhd.v1.reserved = */file_reader_rb32(mov->fp);
		tkhd.v1.duration = file_reader_rb64(mov->fp);
	}
	else
	{
		assert(0 == tkhd.version);
		tkhd.v0.creation_time = file_reader_rb32(mov->fp);
		tkhd.v0.modification_time = file_reader_rb32(mov->fp);
		tkhd.v0.track_ID = file_reader_rb32(mov->fp);
		/*tkhd.v0.reserved = */file_reader_rb32(mov->fp);
		tkhd.v0.duration = file_reader_rb32(mov->fp);
	}

	// const unsigned int(32)[2] reserved = 0;
	file_reader_seek(mov->fp, 8);
	tkhd.layer = (uint16_t)file_reader_rb16(mov->fp);
	tkhd.alternate_group = (uint16_t)file_reader_rb16(mov->fp);
	tkhd.volume = (uint16_t)file_reader_rb16(mov->fp);
	// const unsigned int(16) reserved = 0;
	file_reader_seek(mov->fp, 2);
	for (i = 0; i < 9; i++)
		tkhd.matrix[i] = file_reader_rb32(mov->fp);
	tkhd.width = file_reader_rb32(mov->fp);
	tkhd.height = file_reader_rb32(mov->fp);
	return 0;
}
