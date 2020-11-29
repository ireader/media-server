#include "mov-internal.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// stsd: Sample Description Box

int mp4_read_extra(struct mov_t* mov, const struct mov_box_t* box)
{
	int r;
	uint64_t p1, p2;
	p1 = mov_buffer_tell(&mov->io);
	r = mov_reader_box(mov, box);
	p2 = mov_buffer_tell(&mov->io);
	mov_buffer_skip(&mov->io, box->size - (p2 - p1));
	return r;
}

/*
aligned(8) abstract class SampleEntry (unsigned int(32) format) 
	extends Box(format){ 
	const unsigned int(8)[6] reserved = 0; 
	unsigned int(16) data_reference_index; 
}
*/
static int mov_read_sample_entry(struct mov_t* mov, struct mov_box_t* box, uint16_t* data_reference_index)
{
	box->size = mov_buffer_r32(&mov->io);
	box->type = mov_buffer_r32(&mov->io);
	mov_buffer_skip(&mov->io, 6); // const unsigned int(8)[6] reserved = 0;
	*data_reference_index = (uint16_t)mov_buffer_r16(&mov->io); // ref [dref]
	return 0;
}

/*
class AudioSampleEntry(codingname) extends SampleEntry (codingname){ 
	const unsigned int(32)[2] reserved = 0; 
	template unsigned int(16) channelcount = 2; 
	template unsigned int(16) samplesize = 16; 
	unsigned int(16) pre_defined = 0; 
	const unsigned int(16) reserved = 0 ; 
	template unsigned int(32) samplerate = { default samplerate of media}<<16; 
}
*/
static int mov_read_audio(struct mov_t* mov, struct mov_sample_entry_t* entry)
{
	uint16_t qtver;
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box, &entry->data_reference_index);
    entry->object_type_indication = mov_tag_to_object(box.type);
    entry->stream_type = MP4_STREAM_AUDIO;
	mov->track->tag = box.type;

#if 0
	// const unsigned int(32)[2] reserved = 0;
	mov_buffer_skip(&mov->io, 8);
#else
	qtver = mov_buffer_r16(&mov->io); /* version */
	mov_buffer_r16(&mov->io); /* revision level */
	mov_buffer_r32(&mov->io); /* vendor */
#endif

    entry->u.audio.channelcount = (uint16_t)mov_buffer_r16(&mov->io);
    entry->u.audio.samplesize = (uint16_t)mov_buffer_r16(&mov->io);

#if 0
	// unsigned int(16) pre_defined = 0; 
	// const unsigned int(16) reserved = 0 ;
	mov_buffer_skip(&mov->io, 4);
#else
	mov_buffer_r16(&mov->io); /* audio cid */
	mov_buffer_r16(&mov->io); /* packet size = 0 */
#endif

    entry->u.audio.samplerate = mov_buffer_r32(&mov->io); // { default samplerate of media}<<16;

	// audio extra(avc1: ISO/IEC 14496-14:2003(E))
	box.size -= 36;

	// https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/QTFFChap3/qtff3.html#//apple_ref/doc/uid/TP40000939-CH205-124774
	if (1 == qtver && box.size >= 16)
	{
		// Sound Sample Description (Version 1)
		mov_buffer_r32(&mov->io); // Samples per packet
		mov_buffer_r32(&mov->io); // Bytes per packet
		mov_buffer_r32(&mov->io); // Bytes per frame
		mov_buffer_r32(&mov->io); // Bytes per sample
		box.size -= 16;
	}
	else if (2 == qtver && box.size >= 36)
	{
		// Sound Sample Description (Version 2)
		mov_buffer_r32(&mov->io); // sizeOfStructOnly
		mov_buffer_r64(&mov->io); // audioSampleRate
		mov_buffer_r32(&mov->io); // numAudioChannels
		mov_buffer_r32(&mov->io); // always7F000000
		mov_buffer_r32(&mov->io); // constBitsPerChannel
		mov_buffer_r32(&mov->io); // formatSpecificFlags
		mov_buffer_r32(&mov->io); // constBytesPerAudioPacket
		mov_buffer_r32(&mov->io); // constLPCMFramesPerAudioPacket
		box.size -= 36;
	}

	return mp4_read_extra(mov, &box);
}

/*
class VisualSampleEntry(codingname) extends SampleEntry (codingname){ 
	unsigned int(16) pre_defined = 0; 
	const unsigned int(16) reserved = 0; 
	unsigned int(32)[3] pre_defined = 0; 
	unsigned int(16) width; 
	unsigned int(16) height; 
	template unsigned int(32) horizresolution = 0x00480000; // 72 dpi 
	template unsigned int(32) vertresolution = 0x00480000; // 72 dpi 
	const unsigned int(32) reserved = 0; 
	template unsigned int(16) frame_count = 1; 
	string[32] compressorname; 
	template unsigned int(16) depth = 0x0018; 
	int(16) pre_defined = -1; 
	// other boxes from derived specifications 
	CleanApertureBox clap; // optional 
	PixelAspectRatioBox pasp; // optional 
}
class AVCSampleEntry() extends VisualSampleEntry ('avc1'){
	AVCConfigurationBox config;
	MPEG4BitRateBox (); // optional
	MPEG4ExtensionDescriptorsBox (); // optional
}
class AVC2SampleEntry() extends VisualSampleEntry ('avc2'){
	AVCConfigurationBox avcconfig;
	MPEG4BitRateBox bitrate; // optional
	MPEG4ExtensionDescriptorsBox descr; // optional
	extra_boxes boxes; // optional
}
*/
static int mov_read_video(struct mov_t* mov, struct mov_sample_entry_t* entry)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box, &entry->data_reference_index);
    entry->object_type_indication = mov_tag_to_object(box.type);
    entry->stream_type = MP4_STREAM_VISUAL;
	mov->track->tag = box.type;
#if 1
	 //unsigned int(16) pre_defined = 0; 
	 //const unsigned int(16) reserved = 0;
	 //unsigned int(32)[3] pre_defined = 0;
	mov_buffer_skip(&mov->io, 16);
#else
	mov_buffer_r16(&mov->io); /* version */
	mov_buffer_r16(&mov->io); /* revision level */
	mov_buffer_r32(&mov->io); /* vendor */
	mov_buffer_r32(&mov->io); /* temporal quality */
	mov_buffer_r32(&mov->io); /* spatial quality */
#endif
    entry->u.visual.width = (uint16_t)mov_buffer_r16(&mov->io);
    entry->u.visual.height = (uint16_t)mov_buffer_r16(&mov->io);
    entry->u.visual.horizresolution = mov_buffer_r32(&mov->io); // 0x00480000 - 72 dpi
    entry->u.visual.vertresolution = mov_buffer_r32(&mov->io); // 0x00480000 - 72 dpi
	// const unsigned int(32) reserved = 0;
	mov_buffer_r32(&mov->io); /* data size, always 0 */
    entry->u.visual.frame_count = (uint16_t)mov_buffer_r16(&mov->io);

	//string[32] compressorname;
	//uint32_t len = mov_buffer_r8(&mov->io);
	//mov_buffer_skip(&mov->io, len);
	mov_buffer_skip(&mov->io, 32);

    entry->u.visual.depth = (uint16_t)mov_buffer_r16(&mov->io);
	// int(16) pre_defined = -1;
	mov_buffer_skip(&mov->io, 2);

	// video extra(avc1: ISO/IEC 14496-15:2010(E))
	box.size -= 86;
	return mp4_read_extra(mov, &box);
}

/*
class PixelAspectRatioBox extends Box(‘pasp?{
	unsigned int(32) hSpacing;
	unsigned int(32) vSpacing;
}
*/
int mov_read_pasp(struct mov_t* mov, const struct mov_box_t* box)
{
	mov_buffer_r32(&mov->io);
	mov_buffer_r32(&mov->io);

	(void)box;
	return 0;
}

static int mov_read_hint_sample_entry(struct mov_t* mov, struct mov_sample_entry_t* entry)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box, &entry->data_reference_index);
	mov_buffer_skip(&mov->io, box.size - 16);
	mov->track->tag = box.type;
	return mov_buffer_error(&mov->io);
}

static int mov_read_meta_sample_entry(struct mov_t* mov, struct mov_sample_entry_t* entry)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box, &entry->data_reference_index);
	mov_buffer_skip(&mov->io, box.size - 16);
	mov->track->tag = box.type;
	return mov_buffer_error(&mov->io);
}

// ISO/IEC 14496-12:2015(E) 12.5 Text media (p184)
/*
class PlainTextSampleEntry(codingname) extends SampleEntry (codingname) {
}
class SimpleTextSampleEntry(codingname) extends PlainTextSampleEntry ('stxt') {
	string content_encoding; // optional
	string mime_format;
	BitRateBox (); // optional
	TextConfigBox (); // optional
}
*/
static int mov_read_text_sample_entry(struct mov_t* mov, struct mov_sample_entry_t* entry)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box, &entry->data_reference_index);
	if (MOV_TEXT == box.type)
	{
		// https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/QTFFChap3/qtff3.html#//apple_ref/doc/uid/TP40000939-CH205-69835
		//mov_buffer_r32(&mov->io); /* display flags */
		//mov_buffer_r32(&mov->io); /* text justification */
		//mov_buffer_r16(&mov->io); /* background color: 48-bit RGB color */
		//mov_buffer_r16(&mov->io);
		//mov_buffer_r16(&mov->io);
		//mov_buffer_r64(&mov->io); /* default text box (top, left, bottom, right) */
		//mov_buffer_r64(&mov->io); /* reserved */
		//mov_buffer_r16(&mov->io); /* font number */
		//mov_buffer_r16(&mov->io); /* font face */
		//mov_buffer_r8(&mov->io); /* reserved */
		//mov_buffer_r16(&mov->io); /* reserved */
		//mov_buffer_r16(&mov->io); /* foreground  color: 48-bit RGB color */
		//mov_buffer_r16(&mov->io);
		//mov_buffer_r16(&mov->io);
		////mov_buffer_r16(&mov->io); /* text name */
		mov_buffer_skip(&mov->io, box.size - 16);
	}
	else
	{
		mov_buffer_skip(&mov->io, box.size - 16);
	}

	mov->track->tag = box.type;
	return mov_buffer_error(&mov->io);
}

// ISO/IEC 14496-12:2015(E) 12.6 Subtitle media (p185)
/*
class SubtitleSampleEntry(codingname) extends SampleEntry (codingname) {
}
class XMLSubtitleSampleEntry() extends SubtitleSampleEntry('stpp') {
	string namespace;
	string schema_location; // optional
	string auxiliary_mime_types;
	// optional, required if auxiliary resources are present
	BitRateBox (); // optional
}
class TextSubtitleSampleEntry() extends SubtitleSampleEntry('sbtt') {
	string content_encoding; // optional
	string mime_format;
	BitRateBox (); // optional
	TextConfigBox (); // optional
}
class TextSampleEntry() extends SampleEntry('tx3g') {
	unsigned int(32) displayFlags;
	signed int(8) horizontal-justification;
	signed int(8) vertical-justification;
	unsigned int(8) background-color-rgba[4];
	BoxRecord default-text-box;
	StyleRecord default-style;
	FontTableBox font-table;
	DisparityBox default-disparity;
}
*/
static int mov_read_subtitle_sample_entry(struct mov_t* mov, struct mov_sample_entry_t* entry)
{
	struct mov_box_t box;
	mov_read_sample_entry(mov, &box, &entry->data_reference_index);
	box.size -= 16;
	if (box.type == MOV_TAG('t', 'x', '3', 'g'))
	{
		mov_read_tx3g(mov, &box);
	}
	else
	{
		mov_buffer_skip(&mov->io, box.size - 16);
	}

    entry->object_type_indication = MOV_OBJECT_TEXT;
    entry->stream_type = MP4_STREAM_VISUAL;
	mov->track->tag = box.type;
	return mov_buffer_error(&mov->io);
}

int mov_read_stsd(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_track_t* track = mov->track;

	mov_buffer_r8(&mov->io);
	mov_buffer_r24(&mov->io);
	entry_count = mov_buffer_r32(&mov->io);

	if (track->stsd.entry_count < entry_count)
	{
		void* p = realloc(track->stsd.entries, sizeof(track->stsd.entries[0]) * entry_count);
		if (NULL == p) return ENOMEM;
		track->stsd.entries = (struct mov_sample_entry_t*)p;
	}

	track->stsd.entry_count = entry_count;
	for (i = 0; i < entry_count; i++)
	{
        track->stsd.current = &track->stsd.entries[i];
		memset(track->stsd.current, 0, sizeof(*track->stsd.current));
		if (MOV_AUDIO == track->handler_type)
		{
			mov_read_audio(mov, &track->stsd.entries[i]);
		}
		else if (MOV_VIDEO == track->handler_type)
		{
			mov_read_video(mov, &track->stsd.entries[i]);
		}
		else if (MOV_HINT == track->handler_type)
		{
			mov_read_hint_sample_entry(mov, &track->stsd.entries[i]);
		}
		else if (MOV_META == track->handler_type)
		{
			mov_read_meta_sample_entry(mov, &track->stsd.entries[i]);
		}
		else if (MOV_CLCP == track->handler_type)
		{
			mov_read_meta_sample_entry(mov, &track->stsd.entries[i]);
		}
		else if (MOV_TEXT == track->handler_type)
		{
			mov_read_text_sample_entry(mov, &track->stsd.entries[i]);
		}
		else if (MOV_SUBT == track->handler_type || MOV_SBTL == track->handler_type)
		{
			mov_read_subtitle_sample_entry(mov, &track->stsd.entries[i]);
		}
		else if (MOV_ALIS == track->handler_type)
		{
			mov_read_meta_sample_entry(mov, &track->stsd.entries[i]);
		}
		else
		{
			assert(0); // ignore
			mov_read_meta_sample_entry(mov, &track->stsd.entries[i]);
		}
	}

	(void)box;
	return mov_buffer_error(&mov->io);
}

//static int mov_write_h264(const struct mov_t* mov)
//{
//	size_t size;
//	uint64_t offset;
//	const struct mov_track_t* track = mov->track;
//
//	size = 8 /* Box */;
//
//	offset = mov_buffer_tell(&mov->io);
//	mov_buffer_w32(&mov->io, 0); /* size */
//	mov_buffer_w32(&mov->io, MOV_TAG('a', 'v', 'c', 'C'));
//
//	mov_write_size(mov, offset, size); /* update size */
//	return size;
//}

static size_t mov_write_video(const struct mov_t* mov, const struct mov_sample_entry_t* entry)
{
	size_t size;
	uint64_t offset;
    char compressorname[32];
    memset(compressorname, 0, sizeof(compressorname));
	assert(1 == entry->data_reference_index);

	size = 8 /* Box */ + 8 /* SampleEntry */ + 70 /* VisualSampleEntry */;

	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_w32(&mov->io, mov->track->tag); // "h264"

	mov_buffer_w32(&mov->io, 0); /* Reserved */
	mov_buffer_w16(&mov->io, 0); /* Reserved */
	mov_buffer_w16(&mov->io, entry->data_reference_index); /* Data-reference index */

	mov_buffer_w16(&mov->io, 0); /* Reserved / Codec stream version */
	mov_buffer_w16(&mov->io, 0); /* Reserved / Codec stream revision (=0) */
	mov_buffer_w32(&mov->io, 0); /* Reserved */
	mov_buffer_w32(&mov->io, 0); /* Reserved */
	mov_buffer_w32(&mov->io, 0); /* Reserved */

	mov_buffer_w16(&mov->io, entry->u.visual.width); /* Video width */
	mov_buffer_w16(&mov->io, entry->u.visual.height); /* Video height */
	mov_buffer_w32(&mov->io, 0x00480000); /* Horizontal resolution 72dpi */
	mov_buffer_w32(&mov->io, 0x00480000); /* Vertical resolution 72dpi */
	mov_buffer_w32(&mov->io, 0); /* reserved / Data size (= 0) */
	mov_buffer_w16(&mov->io, 1); /* Frame count (= 1) */

	// ISO 14496-15:2017 AVCC \012AVC Coding
	// ISO 14496-15:2017 HVCC \013HEVC Coding
	//mov_buffer_w8(&mov->io, 0 /*strlen(compressor_name)*/); /* compressorname */
	mov_buffer_write(&mov->io, compressorname, 32); // fill empty

    // ISO/IEC 14496-15:2017 4.5 Template field used (19)
    // 0x18 - the video sequence is in color with no alpha
    // 0x28 - the video sequence is in grayscale with no alpha
    // 0x20 - the video sequence has alpha (gray or color)
	mov_buffer_w16(&mov->io, 0x18); /* Reserved */
	mov_buffer_w16(&mov->io, 0xffff); /* Reserved */

	if(MOV_OBJECT_H264 == entry->object_type_indication)
		size += mov_write_avcc(mov);
	else if(MOV_OBJECT_MP4V == entry->object_type_indication)
		size += mov_write_esds(mov);
	else if (MOV_OBJECT_HEVC == entry->object_type_indication)
		size += mov_write_hvcc(mov);
	else if (MOV_OBJECT_AV1 == entry->object_type_indication)
		size += mov_write_av1c(mov);
    else if (MOV_OBJECT_VP8 == entry->object_type_indication || MOV_OBJECT_VP9 == entry->object_type_indication)
        size += mov_write_vpcc(mov);

	mov_write_size(mov, offset, size); /* update size */
	return size;
}

static size_t mov_write_audio(const struct mov_t* mov, const struct mov_sample_entry_t* entry)
{
	size_t size;
	uint64_t offset;

	size = 8 /* Box */ + 8 /* SampleEntry */ + 20 /* AudioSampleEntry */;

	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_w32(&mov->io, mov->track->tag); // "mp4a"

	mov_buffer_w32(&mov->io, 0); /* Reserved */
	mov_buffer_w16(&mov->io, 0); /* Reserved */
	mov_buffer_w16(&mov->io, 1); /* Data-reference index */

	/* SoundDescription */
	mov_buffer_w16(&mov->io, 0); /* Version */
	mov_buffer_w16(&mov->io, 0); /* Revision level */
	mov_buffer_w32(&mov->io, 0); /* Reserved */

	mov_buffer_w16(&mov->io, entry->u.audio.channelcount); /* channelcount */
	mov_buffer_w16(&mov->io, entry->u.audio.samplesize); /* samplesize */

	mov_buffer_w16(&mov->io, 0); /* pre_defined */
	mov_buffer_w16(&mov->io, 0); /* reserved / packet size (= 0) */

	// https://www.opus-codec.org/docs/opus_in_isobmff.html
	// 4.3 Definitions of Opus sample
	// OpusSampleEntry: 
	// 1. The samplesize field shall be set to 16.
	// 2. The samplerate field shall be set to 48000<<16.
	mov_buffer_w32(&mov->io, entry->u.audio.samplerate); /* samplerate */

	if(MOV_OBJECT_AAC == entry->object_type_indication || MOV_OBJECT_MP3 == entry->object_type_indication || MOV_OBJECT_MP1A == entry->object_type_indication)
		size += mov_write_esds(mov);
    else if(MOV_OBJECT_OPUS == entry->object_type_indication)
        size += mov_write_dops(mov);

	mov_write_size(mov, offset, size); /* update size */
	return size;
}

static int mov_write_subtitle(const struct mov_t* mov, const struct mov_sample_entry_t* entry)
{
	int size;
	uint64_t offset;

	size = 8 /* Box */ + 8 /* SampleEntry */ + entry->extra_data_size;

	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_w32(&mov->io, mov->track->tag); // "tx3g"

	mov_buffer_w32(&mov->io, 0); /* Reserved */
	mov_buffer_w16(&mov->io, 0); /* Reserved */
	mov_buffer_w16(&mov->io, entry->data_reference_index); /* Data-reference index */

	if (MOV_TAG('t', 'x', '3', 'g') == mov->track->tag)
	{
		size += mov_write_tx3g(mov);
	}
	else if (entry->extra_data_size > 0) // unknown type
	{
		mov_buffer_write(&mov->io, entry->extra_data, entry->extra_data_size);
		size += entry->extra_data_size;
	}

	mov_write_size(mov, offset, size); /* update size */
	return size;
}

size_t mov_write_stsd(const struct mov_t* mov)
{
	uint32_t i;
	size_t size;
	uint64_t offset;
	const struct mov_track_t* track = mov->track;

	size = 12 /* full box */ + 4 /* entry count */;

	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "stsd", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	mov_buffer_w32(&mov->io, track->stsd.entry_count); /* entry count */

	for (i = 0; i < track->stsd.entry_count; i++)
	{
        ((struct mov_track_t*)track)->stsd.current = &track->stsd.entries[i];

		if (MOV_VIDEO == track->handler_type)
		{
			size += mov_write_video(mov, &track->stsd.entries[i]);
		}
		else if (MOV_AUDIO == track->handler_type)
		{
			size += mov_write_audio(mov, &track->stsd.entries[i]);
		}
		else if (MOV_SUBT == track->handler_type || MOV_TEXT == track->handler_type || MOV_SBTL == track->handler_type)
		{
			size += mov_write_subtitle(mov, &track->stsd.entries[i]);
		}
		else
		{
			assert(0);
		}
	}

	mov_write_size(mov, offset, size); /* update size */
	return size;
}
