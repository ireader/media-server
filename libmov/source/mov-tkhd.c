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
    uint8_t version;
    uint32_t flags;
    uint64_t creation_time;
    uint64_t modification_time;
    uint64_t duration;
    uint32_t track_ID;
	struct mov_tkhd_t* tkhd;
    struct mov_track_t* track;

	version = mov_buffer_r8(&mov->io);
	flags = mov_buffer_r24(&mov->io);

	if (1 == version)
	{
		creation_time = mov_buffer_r64(&mov->io);
		modification_time = mov_buffer_r64(&mov->io);
		track_ID = mov_buffer_r32(&mov->io);
		/*reserved = */mov_buffer_r32(&mov->io);
		duration = mov_buffer_r64(&mov->io);
	}
	else
	{
		assert(0 == version);
		creation_time = mov_buffer_r32(&mov->io);
		modification_time = mov_buffer_r32(&mov->io);
		track_ID = mov_buffer_r32(&mov->io);
		/*reserved = */mov_buffer_r32(&mov->io);
		duration = mov_buffer_r32(&mov->io);
	}
    mov_buffer_skip(&mov->io, 8); // const unsigned int(32)[2] reserved = 0;

    track = mov_fetch_track(mov, track_ID);
    if (NULL == track) return -1;

    mov->track = track;
    tkhd = &mov->track->tkhd;
	tkhd->version = version;
    tkhd->flags = flags;
    tkhd->duration = duration;
    tkhd->creation_time = creation_time;
    tkhd->modification_time = modification_time;

	tkhd->layer = (int16_t)mov_buffer_r16(&mov->io);
	tkhd->alternate_group = (int16_t)mov_buffer_r16(&mov->io);
	tkhd->volume = (int16_t)mov_buffer_r16(&mov->io);
	mov_buffer_skip(&mov->io, 2); // const unsigned int(16) reserved = 0;
	for (i = 0; i < 9; i++)
		tkhd->matrix[i] = mov_buffer_r32(&mov->io);
	tkhd->width = mov_buffer_r32(&mov->io);
	tkhd->height = mov_buffer_r32(&mov->io);

	(void)box;
	return 0;
}

size_t mov_write_tkhd(const struct mov_t* mov)
{
//	int rotation = 0; // 90/180/270
	uint16_t group = 0;
	const struct mov_tkhd_t* tkhd = &mov->track->tkhd;

	mov_buffer_w32(&mov->io, 92); /* size */
	mov_buffer_write(&mov->io, "tkhd", 4);
	mov_buffer_w8(&mov->io, 0); /* version */
	mov_buffer_w24(&mov->io, tkhd->flags); /* flags */

	mov_buffer_w32(&mov->io, (uint32_t)tkhd->creation_time); /* creation_time */
	mov_buffer_w32(&mov->io, (uint32_t)tkhd->modification_time); /* modification_time */
	mov_buffer_w32(&mov->io, tkhd->track_ID); /* track_ID */
	mov_buffer_w32(&mov->io, 0); /* reserved */
	mov_buffer_w32(&mov->io, (uint32_t)tkhd->duration); /* duration */

	mov_buffer_w32(&mov->io, 0); /* reserved */
	mov_buffer_w32(&mov->io, 0); /* reserved */
	mov_buffer_w16(&mov->io, tkhd->layer); /* layer */
	mov_buffer_w16(&mov->io, group); /* alternate_group */
	//mov_buffer_w16(&mov->io, AVSTREAM_AUDIO == track->stream_type ? 0x0100 : 0); /* volume */
	mov_buffer_w16(&mov->io, tkhd->volume); /* volume */
	mov_buffer_w16(&mov->io, 0); /* reserved */

	// matrix
	//for (i = 0; i < 9; i++)
	//	file_reader_rb32(mov->fp, tkhd->matrix[i]);
	mov_buffer_w32(&mov->io, 0x00010000); /* u */
	mov_buffer_w32(&mov->io, 0);
	mov_buffer_w32(&mov->io, 0);
	mov_buffer_w32(&mov->io, 0); /* v */
	mov_buffer_w32(&mov->io, 0x00010000);
	mov_buffer_w32(&mov->io, 0);
	mov_buffer_w32(&mov->io, 0); /* w */
	mov_buffer_w32(&mov->io, 0);
	mov_buffer_w32(&mov->io, 0x40000000);

	mov_buffer_w32(&mov->io, tkhd->width /*track->av.video.width * 0x10000U*/); /* width */
	mov_buffer_w32(&mov->io, tkhd->height/*track->av.video.height * 0x10000U*/); /* height */
	return 92;
}
