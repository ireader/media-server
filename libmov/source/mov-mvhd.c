#include "mov-internal.h"
#include <assert.h>

// ISO/IEC 14496-12:2012(E)
// 8.2.2.1 Movie Header Box (p30)
// Box Type : 'mvhd' 
// Container : Movie Box('moov') 
// Mandatory : Yes 
// Quantity : Exactly one

/*
aligned(8) class MovieHeaderBox extends FullBox('mvhd', version, 0) { 
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
	struct mov_mvhd_t* mvhd = &mov->mvhd;

	mvhd->version = mov_buffer_r8(&mov->io);
	mvhd->flags = mov_buffer_r24(&mov->io);

	if (1 == mvhd->version)
	{
		mvhd->creation_time = mov_buffer_r64(&mov->io);
		mvhd->modification_time = mov_buffer_r64(&mov->io);
		mvhd->timescale = mov_buffer_r32(&mov->io);
		mvhd->duration = mov_buffer_r64(&mov->io);
	}
	else
	{
		assert(0 == mvhd->version);
		mvhd->creation_time = mov_buffer_r32(&mov->io);
		mvhd->modification_time = mov_buffer_r32(&mov->io);
		mvhd->timescale = mov_buffer_r32(&mov->io);
		mvhd->duration = mov_buffer_r32(&mov->io);
	}

	mvhd->rate = mov_buffer_r32(&mov->io);
	mvhd->volume = (uint16_t)mov_buffer_r16(&mov->io);
	//mvhd->reserved = mov_buffer_r16(&mov->io);
	//mvhd->reserved2[0] = mov_buffer_r32(&mov->io);
	//mvhd->reserved2[1] = mov_buffer_r32(&mov->io);
	mov_buffer_skip(&mov->io, 10);
	for (i = 0; i < 9; i++)
		mvhd->matrix[i] = mov_buffer_r32(&mov->io);
#if 0
	for (i = 0; i < 6; i++)
		mvhd->pre_defined[i] = mov_buffer_r32(&mov->io);
#else
	mov_buffer_r32(&mov->io); /* preview time */
	mov_buffer_r32(&mov->io); /* preview duration */
	mov_buffer_r32(&mov->io); /* poster time */
	mov_buffer_r32(&mov->io); /* selection time */
	mov_buffer_r32(&mov->io); /* selection duration */
	mov_buffer_r32(&mov->io); /* current time */
#endif
	mvhd->next_track_ID = mov_buffer_r32(&mov->io);

	(void)box;
	return 0;
}

size_t mov_write_mvhd(const struct mov_t* mov)
{
//	int rotation = 0; // 90/180/270
	const struct mov_mvhd_t* mvhd = &mov->mvhd;

	mov_buffer_w32(&mov->io, 108); /* size */
	mov_buffer_write(&mov->io, "mvhd", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */

	mov_buffer_w32(&mov->io, (uint32_t)mvhd->creation_time); /* creation_time */
	mov_buffer_w32(&mov->io, (uint32_t)mvhd->modification_time); /* modification_time */
	mov_buffer_w32(&mov->io, mvhd->timescale); /* timescale */
	mov_buffer_w32(&mov->io, (uint32_t)mvhd->duration); /* duration */

	mov_buffer_w32(&mov->io, 0x00010000); /* rate 1.0 */
	mov_buffer_w16(&mov->io, 0x0100); /* volume 1.0 = normal */
	mov_buffer_w16(&mov->io, 0); /* reserved */
	mov_buffer_w32(&mov->io, 0); /* reserved */
	mov_buffer_w32(&mov->io, 0); /* reserved */

	// matrix
	mov_buffer_w32(&mov->io, 0x00010000); /* u */
	mov_buffer_w32(&mov->io, 0);
	mov_buffer_w32(&mov->io, 0);
	mov_buffer_w32(&mov->io, 0); /* v */
	mov_buffer_w32(&mov->io, 0x00010000);
	mov_buffer_w32(&mov->io, 0);
	mov_buffer_w32(&mov->io, 0); /* w */
	mov_buffer_w32(&mov->io, 0);
	mov_buffer_w32(&mov->io, 0x40000000);

	mov_buffer_w32(&mov->io, 0); /* reserved (preview time) */
	mov_buffer_w32(&mov->io, 0); /* reserved (preview duration) */
	mov_buffer_w32(&mov->io, 0); /* reserved (poster time) */
	mov_buffer_w32(&mov->io, 0); /* reserved (selection time) */
	mov_buffer_w32(&mov->io, 0); /* reserved (selection duration) */
	mov_buffer_w32(&mov->io, 0); /* reserved (current time) */

	mov_buffer_w32(&mov->io, mvhd->next_track_ID); /* Next track id */

	return 108;
}
