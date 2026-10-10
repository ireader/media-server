#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
* ISO/IEC 14496-12
* 
aligned(8) class ChannelLayout extends FullBox('chnl', version, flags=0) {	
	if (version==0) {
		unsigned int(8) stream_structure;
		if (stream_structure & channelStructured) {
			unsigned int(8) definedLayout;
			if (definedLayout==0) {
				for (i = 1 ; i <= layout_channel_count ; i++) {
					//  layout_channel_count comes from the sample entry
					unsigned int(8) speaker_position;
					if (speaker_position == 126) {	// explicit position
						signed int (16) azimuth;
						signed int (8)  elevation;
					}
				}
			} else {
				unsigned int(64)	omittedChannelsMap;
						// a ¡®1¡¯ bit indicates ¡®not in this track¡¯
			}
		}
		if (stream_structure & objectStructured) {
			unsigned int(8) object_count;
		}
	} else {
		unsigned int(4) stream_structure;
		unsigned int(4) format_ordering;
		unsigned int(8) baseChannelCount;
		if (stream_structure & channelStructured) {
			unsigned int(8) definedLayout;
			if (definedLayout==0) {
				unsigned int(8) layout_channel_count;
				for (i = 1 ; i <= layout_channel_count ; i++) {
					unsigned int(8) speaker_position;
					if (speaker_position == 126) {	// explicit position
						signed int (16) azimuth;
						signed int (8)  elevation;
					}
				}
			} else {
				int(4) reserved = 0;
				unsigned int(3) channel_order_definition;
				unsigned int(1) omitted_channels_present;
				if (omitted_channels_present == 1) {
					unsigned int(64)	omittedChannelsMap;
							// a ¡®1¡¯ bit indicates ¡®not in this track¡¯
				}
			}
		}
		if (stream_structure & objectStructured) {
							// object_count is derived from baseChannelCount
		}
	}
}
*/

int mov_read_chnl(struct mov_t* mov, const struct mov_box_t* box)
{
	uint8_t version, stream_structure, speaker_position;
	uint8_t layout_channel_count, i;
	struct mov_track_t* track = mov->track;
	struct mov_sample_entry_t* entry = track->stsd.current;

	version = mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */

	if (0 == version)
	{
		stream_structure = mov_buffer_r8(&mov->io);
		if (stream_structure & 0x01) // Channels, possibly followed by Objects
		{
			if (0 == mov_buffer_r8(&mov->io)) // definedLayout
			{
				for (i = 0; i < entry->u.audio.channelcount; i++)
				{
					speaker_position = mov_buffer_r8(&mov->io);
					if (126 == speaker_position)
					{
						mov_buffer_r16(&mov->io); /* azimuth */
						mov_buffer_r8(&mov->io); /* elevation */
					}
				}
			}
			else
			{
				mov_buffer_r64(&mov->io); /* omittedChannelsMap */
			}
		}
		if (stream_structure & 0x02) // Objects, possibly followed by Channels
		{
			mov_buffer_r8(&mov->io); /* object_count */
		}
	}
	else
	{
		stream_structure = mov_buffer_r8(&mov->io); /* stream_structure & format_ordering */
		mov_buffer_r8(&mov->io); /* baseChannelCount */
		if (stream_structure & 0x01) // Channels, possibly followed by Objects
		{
			if (0 == mov_buffer_r8(&mov->io)) // definedLayout
			{
				layout_channel_count = mov_buffer_r8(&mov->io);
				for (i = 0; i < layout_channel_count; i++)
				{
					speaker_position = mov_buffer_r8(&mov->io);
					if (126 == speaker_position)
					{
						mov_buffer_r16(&mov->io); /* azimuth */
						mov_buffer_r8(&mov->io); /* elevation */
					}
				}
			}
			else
			{
				if (mov_buffer_r8(&mov->io) & 0x01) /* channel_order_definition & omitted_channels_present */
				{
					mov_buffer_r64(&mov->io); /* omittedChannelsMap */
				}
			}
		}

		if (stream_structure & 0x02) // Objects, possibly followed by Channels
		{
		}
	}
}

size_t mov_write_chnl(const struct mov_t* mov)
{
	const struct mov_track_t* track = mov->track;
	const struct mov_sample_entry_t* entry = track->stsd.current;
	mov_buffer_w32(&mov->io, 10 + 12); /* size */
	mov_buffer_write(&mov->io, "chnl", 4);
	mov_buffer_w8(&mov->io, 0); /* version */
	mov_buffer_w24(&mov->io, 0); /* flags */

	mov_buffer_w8(&mov->io, 1); /* stream_structure */
	mov_buffer_w8(&mov->io, 1); /* definedLayout */
	mov_buffer_w64(&mov->io, 0); /* omittedChannelsMap */
	return 10 + 12;
}
