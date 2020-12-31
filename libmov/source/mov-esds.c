#include "mov-internal.h"
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

static int mp4_read_tag(struct mov_t* mov, uint64_t bytes);

// ISO/IEC 14496-1:2010(E)
// 7.2.2 Common data structures
// Table-1 List of Class Tags for Descriptors (p31)
enum {
	ISO_ObjectDescrTag = 0x01,
	ISO_InitialObjectDescrTag = 0x02,
	ISO_ESDescrTag = 0x03,
	ISO_DecoderConfigDescrTag = 0x04,
	ISO_DecSpecificInfoTag = 0x05,
	ISO_SLConfigDescrTag = 0x06,
	ISO_ContentIdentDescrTag = 0x07,
	ISO_SupplContentIdentDescrTag = 0x08,
	ISO_IPI_DescrPointerTag = 0x09,
	ISO_IPMP_DescrPointerTag = 0x0A,
	ISO_IPMP_DescrTag = 0x0B,
	ISO_QoS_DescrTag = 0x0C,
	ISO_RegistrationDescrTag = 0x0D,
	ISO_ES_ID_IncTag = 0x0E,
	ISO_ES_ID_RefTag = 0x0F,
	ISO_MP4_IOD_Tag = 0x10,
	ISO_MP4_OD_Tag = 0x11,
};

// ISO/IEC 14496-1:2010(E)
// 7.2.2.3 BaseCommand
// Table-2 List of Class Tags for Commands (p33)
enum {
	ISO_ObjectDescrUpdateTag = 0x01,
	ISO_ObjectDescrRemoveTag = 0x02,
	ISO_ES_DescrUpdateTag = 0x03,
	ISO_ES_DescrRemoveTag = 0x04,
	ISO_IPMP_DescrUpdateTag = 0x05,
	ISO_IPMP_DescrRemoveTag = 0x06,
	ISO_ES_DescrRemoveRefTag = 0x07,
	ISO_ObjectDescrExecuteTag = 0x08,
	ISO_User_Private = 0xC0,
};

// ISO/IEC 14496-1:2010(E) 7.2.2.2 BaseDescriptor (p32)
// ISO/IEC 14496-1:2010(E) 8.3.3 Expandable classes (p116)
/*
abstract aligned(8) expandable(2^28-1) class BaseDescriptor : bit(8) tag=0 {
	// empty. To be filled by classes extending this class.
}

int sizeOfInstance = 0;
bit(1) nextByte;
bit(7) sizeOfInstance;
while(nextByte) {
	bit(1) nextByte;
	bit(7) sizeByte;
	sizeOfInstance = sizeOfInstance<<7 | sizeByte;
}
*/
static int mov_read_base_descr(struct mov_t* mov, int bytes, int* tag,  int* len)
{
	int i;
	uint32_t c;

	*tag = mov_buffer_r8(&mov->io);
	*len = 0;
	c = 0x80;
	for (i = 0; i < 4 && i + 1 < bytes && 0 != (c & 0x80); i++)
	{
		c = mov_buffer_r8(&mov->io);
		*len = (*len << 7) | (c & 0x7F);
		//if (0 == (c & 0x80))
		//	break;
	}
	return 1 + i;
}

static uint32_t mov_write_base_descr(const struct mov_t* mov, uint8_t tag, uint32_t len)
{
	mov_buffer_w8(&mov->io, tag);
	mov_buffer_w8(&mov->io, (uint8_t)(0x80 | (len >> 21)));
	mov_buffer_w8(&mov->io, (uint8_t)(0x80 | (len >> 14)));
	mov_buffer_w8(&mov->io, (uint8_t)(0x80 | (len >> 7)));
	mov_buffer_w8(&mov->io, (uint8_t)(0x7F & len));
	return 5;
}

// ISO/IEC 14496-1:2010(E) 7.2.6.5 ES_Descriptor (p47)
/*
class ES_Descriptor extends BaseDescriptor : bit(8) tag=ES_DescrTag {
	bit(16) ES_ID;
	bit(1) streamDependenceFlag;
	bit(1) URL_Flag;
	bit(1) OCRstreamFlag;
	bit(5) streamPriority;
	if (streamDependenceFlag)
		bit(16) dependsOn_ES_ID;
	if (URL_Flag) {
		bit(8) URLlength;
		bit(8) URLstring[URLlength];
	}
	if (OCRstreamFlag)
		bit(16) OCR_ES_Id;
	DecoderConfigDescriptor decConfigDescr;
	if (ODProfileLevelIndication==0x01) //no SL extension.
	{
		SLConfigDescriptor slConfigDescr;
	}
	else // SL extension is possible.
	{
		SLConfigDescriptor slConfigDescr;
	}
	IPI_DescrPointer ipiPtr[0 .. 1];
	IP_IdentificationDataSet ipIDS[0 .. 255];
	IPMP_DescriptorPointer ipmpDescrPtr[0 .. 255];
	LanguageDescriptor langDescr[0 .. 255];
	QoS_Descriptor qosDescr[0 .. 1];
	RegistrationDescriptor regDescr[0 .. 1];
	ExtensionDescriptor extDescr[0 .. 255];
}
*/
static int mp4_read_es_descriptor(struct mov_t* mov, uint64_t bytes)
{
	uint64_t p1, p2;
	p1 = mov_buffer_tell(&mov->io);
	/*uint32_t ES_ID = */mov_buffer_r16(&mov->io);
	uint32_t flags = mov_buffer_r8(&mov->io);
	if (flags & 0x80) //streamDependenceFlag
		mov_buffer_r16(&mov->io);
	if (flags & 0x40) { //URL_Flag
		uint32_t n = mov_buffer_r8(&mov->io);
		mov_buffer_skip(&mov->io, n);
	}

	if (flags & 0x20) //OCRstreamFlag
		mov_buffer_r16(&mov->io);

	p2 = mov_buffer_tell(&mov->io);
	return mp4_read_tag(mov, bytes - (p2 - p1));
}

// ISO/IEC 14496-1:2010(E) 7.2.6.7 DecoderSpecificInfo (p51)
/*
abstract class DecoderSpecificInfo extends BaseDescriptor : bit(8)
	tag=DecSpecificInfoTag
{
	// empty. To be filled by classes extending this class.
}
*/
static int mp4_read_decoder_specific_info(struct mov_t* mov, int len)
{
	struct mov_track_t* track = mov->track;
	struct mov_sample_entry_t* entry = track->stsd.current;
	if (entry->extra_data_size < len)
	{
		void* p = realloc(entry->extra_data, len);
		if (NULL == p) return ENOMEM;
		entry->extra_data = p;
	}

	mov_buffer_read(&mov->io, entry->extra_data, len);
	entry->extra_data_size = len;
	return mov_buffer_error(&mov->io);
}

static int mp4_write_decoder_specific_info(const struct mov_t* mov)
{
	const struct mov_sample_entry_t* entry = mov->track->stsd.current;
	mov_write_base_descr(mov, ISO_DecSpecificInfoTag, entry->extra_data_size);
	mov_buffer_write(&mov->io, entry->extra_data, entry->extra_data_size);
	return entry->extra_data_size;
}

// ISO/IEC 14496-1:2010(E) 7.2.6.6 DecoderConfigDescriptor (p48)
/*
class DecoderConfigDescriptor extends BaseDescriptor : bit(8) tag=DecoderConfigDescrTag {
	bit(8) objectTypeIndication;
	bit(6) streamType;
	bit(1) upStream;
	const bit(1) reserved=1;
	bit(24) bufferSizeDB;
	bit(32) maxBitrate;
	bit(32) avgBitrate;
	DecoderSpecificInfo decSpecificInfo[0 .. 1];
	profileLevelIndicationIndexDescriptor profileLevelIndicationIndexDescr[0..255];
}
*/
static int mp4_read_decoder_config_descriptor(struct mov_t* mov, int len)
{
	struct mov_sample_entry_t* entry = mov->track->stsd.current;
	entry->object_type_indication = (uint8_t)mov_buffer_r8(&mov->io); /* objectTypeIndication */
	entry->stream_type = (uint8_t)mov_buffer_r8(&mov->io) >> 2; /* stream type */
	/*uint32_t bufferSizeDB = */mov_buffer_r24(&mov->io); /* buffer size db */
	/*uint32_t max_rate = */mov_buffer_r32(&mov->io); /* max bit-rate */
	/*uint32_t bit_rate = */mov_buffer_r32(&mov->io); /* avg bit-rate */
	return mp4_read_tag(mov, (uint64_t)len - 13); // mp4_read_decoder_specific_info
}

static int mp4_write_decoder_config_descriptor(const struct mov_t* mov)
{
	const struct mov_sample_entry_t* entry = mov->track->stsd.current;
	int size = 13 + (entry->extra_data_size > 0 ? entry->extra_data_size + 5 : 0);
	mov_write_base_descr(mov, ISO_DecoderConfigDescrTag, size);
	mov_buffer_w8(&mov->io, entry->object_type_indication);
	mov_buffer_w8(&mov->io, 0x01/*reserved*/ | (entry->stream_type << 2));
	mov_buffer_w24(&mov->io, 0); /* buffer size db */
	mov_buffer_w32(&mov->io, 88360); /* max bit-rate */
	mov_buffer_w32(&mov->io, 88360); /* avg bit-rate */

	if (entry->extra_data_size > 0)
		mp4_write_decoder_specific_info(mov);

	return size;
}

// ISO/IEC 14496-1:2010(E) 7.3.2.3 SL Packet Header Configuration (p92)
/*
class SLConfigDescriptor extends BaseDescriptor : bit(8) tag=SLConfigDescrTag {
	bit(8) predefined;
	if (predefined==0) {
		bit(1) useAccessUnitStartFlag;
		bit(1) useAccessUnitEndFlag;
		bit(1) useRandomAccessPointFlag;
		bit(1) hasRandomAccessUnitsOnlyFlag;
		bit(1) usePaddingFlag;
		bit(1) useTimeStampsFlag;
		bit(1) useIdleFlag;
		bit(1) durationFlag;
		bit(32) timeStampResolution;
		bit(32) OCRResolution;
		bit(8) timeStampLength; // must be ¡Ü 64
		bit(8) OCRLength; // must be ¡Ü 64
		bit(8) AU_Length; // must be ¡Ü 32
		bit(8) instantBitrateLength;
		bit(4) degradationPriorityLength;
		bit(5) AU_seqNumLength; // must be ¡Ü 16
		bit(5) packetSeqNumLength; // must be ¡Ü 16
		bit(2) reserved=0b11;
	}
	if (durationFlag) {
		bit(32) timeScale;
		bit(16) accessUnitDuration;
		bit(16) compositionUnitDuration;
	}
	if (!useTimeStampsFlag) {
		bit(timeStampLength) startDecodingTimeStamp;
		bit(timeStampLength) startCompositionTimeStamp;
	}
}

class ExtendedSLConfigDescriptor extends SLConfigDescriptor : bit(8)
tag=ExtSLConfigDescrTag {
	SLExtensionDescriptor slextDescr[1..255];
}
*/
static int mp4_read_sl_config_descriptor(struct mov_t* mov)
{
	int flags = 0;
	int predefined = mov_buffer_r8(&mov->io);
	if (0 == predefined)
	{
		flags = mov_buffer_r8(&mov->io);
		/*uint32_t timeStampResolution = */mov_buffer_r32(&mov->io);
		/*uint32_t OCRResolution = */mov_buffer_r32(&mov->io);
		/*int timeStampLength = */mov_buffer_r8(&mov->io);
		/*int OCRLength = */mov_buffer_r8(&mov->io);
		/*int AU_Length = */mov_buffer_r8(&mov->io);
		/*int instantBitrateLength = */mov_buffer_r8(&mov->io);
		/*uint16_t length = */mov_buffer_r16(&mov->io);
	}
	else if (1 == predefined) // null SL packet header
	{
		flags = 0x00;
		//int TimeStampResolution = 1000;
		//int timeStampLength = 32;
	}
	else if (2 == predefined) // Reserved for use in MP4 files
	{ 
		// Table 14 ¡ª Detailed predefined SLConfigDescriptor values (p93)
		flags = 0x04;
	}

	// durationFlag
	if (flags & 0x01)
	{
		/*uint32_t timeScale = */mov_buffer_r32(&mov->io);
		/*uint16_t accessUnitDuration = */mov_buffer_r16(&mov->io);
		/*uint16_t compositionUnitDuration = */mov_buffer_r16(&mov->io);
	}

	// useTimeStampsFlag
	if (0 == (flags & 0x04))
	{
		//uint64_t startDecodingTimeStamp = 0; // file_reader_rb8(timeStampLength / 8)
		//uint64_t startCompositionTimeStamp = 0; // file_reader_rb8(timeStampLength / 8)
	}
	return mov_buffer_error(&mov->io);
}

static size_t mp4_write_sl_config_descriptor(const struct mov_t* mov)
{
	size_t size = 1;
	size += mov_write_base_descr(mov, ISO_SLConfigDescrTag, 1);
	mov_buffer_w8(&mov->io, 0x02);
	return size;
}

static int mp4_read_tag(struct mov_t* mov, uint64_t bytes)
{
	int tag, len;
	uint64_t p1, p2, offset;

	for (offset = 0; offset < bytes; offset += len)
	{
		tag = len = 0;
		offset += mov_read_base_descr(mov, (int)(bytes - offset), &tag, &len);
		if (offset + len > bytes)
			break;

		p1 = mov_buffer_tell(&mov->io);
		switch (tag)
		{
		case ISO_ESDescrTag:
			mp4_read_es_descriptor(mov, len);
			break;

		case ISO_DecoderConfigDescrTag:
			mp4_read_decoder_config_descriptor(mov, len);
			break;

		case ISO_DecSpecificInfoTag:
			mp4_read_decoder_specific_info(mov, len);
			break;

		case ISO_SLConfigDescrTag:
			mp4_read_sl_config_descriptor(mov);
			break;

		default:
			break;
		}

		p2 = mov_buffer_tell(&mov->io);
		mov_buffer_skip(&mov->io, len - (p2 - p1));
	}

	return mov_buffer_error(&mov->io);
}

// ISO/IEC 14496-14:2003(E) 5.6 Sample Description Boxes (p15)
int mov_read_esds(struct mov_t* mov, const struct mov_box_t* box)
{
	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	return mp4_read_tag(mov, box->size - 4);
}

static size_t mp4_write_es_descriptor(const struct mov_t* mov)
{
	uint32_t size = 3; // mp4_write_decoder_config_descriptor
	const struct mov_sample_entry_t* entry = mov->track->stsd.current;
	size += 5 + 13 + (entry->extra_data_size > 0 ? entry->extra_data_size + 5 : 0); // mp4_write_decoder_config_descriptor
	size += 5 + 1; // mp4_write_sl_config_descriptor

	size += mov_write_base_descr(mov, ISO_ESDescrTag, size);
	mov_buffer_w16(&mov->io, (uint16_t)mov->track->tkhd.track_ID); // ES_ID
	mov_buffer_w8(&mov->io, 0x00); // flags (= no flags)

	mp4_write_decoder_config_descriptor(mov);
	mp4_write_sl_config_descriptor(mov);
	return size;
}

size_t mov_write_esds(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;

	size = 12 /* full box */;
	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "esds", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */

	size += mp4_write_es_descriptor(mov);

	mov_write_size(mov, offset, size); /* update size */
	return size;
}
