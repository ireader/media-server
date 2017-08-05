#include "file-reader.h"
#include "file-writer.h"
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
*/
static int mov_read_base_descr(struct mov_t* mov, int* tag,  int* len)
{
	int count = 4;

	*tag = file_reader_r8(mov->fp);
	*len = 0;
	while (count-- > 0)
	{
		uint32_t c = file_reader_r8(mov->fp);
		*len = (*len << 7) | (c & 0x7F);
		if (0 == (c & 0x80))
			break;
	}
	return 1 + 4 - count;
}

static size_t mov_write_base_descr(const struct mov_t* mov, uint8_t tag, uint32_t len)
{
	file_writer_w8(mov->fp, tag);
	file_writer_w8(mov->fp, (uint8_t)(0x80 | (len >> 21)));
	file_writer_w8(mov->fp, (uint8_t)(0x80 | (len >> 14)));
	file_writer_w8(mov->fp, (uint8_t)(0x80 | (len >> 7)));
	file_writer_w8(mov->fp, (uint8_t)(0x7F & len));
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
	p1 = file_reader_tell(mov->fp);
	/*uint32_t ES_ID = */file_reader_rb16(mov->fp);
	uint32_t flags = file_reader_r8(mov->fp);
	if (flags & 0x80) //streamDependenceFlag
		file_reader_rb16(mov->fp);
	if (flags & 0x40) { //URL_Flag
		uint32_t n = file_reader_r8(mov->fp);
		file_reader_skip(mov->fp, n);
	}

	if (flags & 0x20) //OCRstreamFlag
		file_reader_rb16(mov->fp);

	p2 = file_reader_tell(mov->fp);
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
static int mp4_read_decoder_specific_info(struct mov_t* mov, size_t len)
{
	struct mov_track_t* track = mov->track;
	if (track->extra_data_size < len)
	{
		void* p = realloc(track->extra_data, len);
		if (NULL == p) return ENOMEM;
		track->extra_data = p;
	}

	file_reader_read(mov->fp, track->extra_data, len);
	track->extra_data_size = len;
	return file_reader_error(mov->fp);
}

static int mp4_write_decoder_specific_info(const struct mov_t* mov)
{
	mov_write_base_descr(mov, ISO_DecSpecificInfoTag, mov->track->extra_data_size);
	file_writer_write(mov->fp, mov->track->extra_data, mov->track->extra_data_size);
	return mov->track->extra_data_size;
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
	mov->track->stsd[0].object_type_indication = (uint8_t)file_reader_r8(mov->fp); /* objectTypeIndication */
	mov->track->stsd[0].stream_type = (uint8_t)file_reader_r8(mov->fp) >> 2; /* stream type */
	/*uint32_t bufferSizeDB = */file_reader_rb24(mov->fp); /* buffer size db */
	/*uint32_t max_rate = */file_reader_rb32(mov->fp); /* max bit-rate */
	/*uint32_t bit_rate = */file_reader_rb32(mov->fp); /* avg bit-rate */
	return mp4_read_tag(mov, len - 13); // mp4_read_decoder_specific_info
}

static int mp4_write_decoder_config_descriptor(const struct mov_t* mov)
{
	size_t size = 13 + (mov->track->extra_data_size > 0 ? mov->track->extra_data_size + 5 : 0);
	mov_write_base_descr(mov, ISO_DecoderConfigDescrTag, size);
	file_writer_w8(mov->fp, mov->track->stsd[0].object_type_indication);
	file_writer_w8(mov->fp, 0x01/*reserved*/ | (mov->track->stsd[0].stream_type << 2));
	file_writer_wb24(mov->fp, 0); /* buffer size db */
	file_writer_wb32(mov->fp, 88360); /* max bit-rate */
	file_writer_wb32(mov->fp, 88360); /* avg bit-rate */

	if (mov->track->extra_data_size > 0)
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
	int predefined = file_reader_r8(mov->fp);
	if (0 == predefined)
	{
		flags = file_reader_r8(mov->fp);
		/*uint32_t timeStampResolution = */file_reader_rb32(mov->fp);
		/*uint32_t OCRResolution = */file_reader_rb32(mov->fp);
		/*int timeStampLength = */file_reader_r8(mov->fp);
		/*int OCRLength = */file_reader_r8(mov->fp);
		/*int AU_Length = */file_reader_r8(mov->fp);
		/*int instantBitrateLength = */file_reader_r8(mov->fp);
		/*uint16_t length = */file_reader_rb16(mov->fp);
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
		/*uint32_t timeScale = */file_reader_rb32(mov->fp);
		/*uint16_t accessUnitDuration = */file_reader_rb16(mov->fp);
		/*uint16_t compositionUnitDuration = */file_reader_rb16(mov->fp);
	}

	// useTimeStampsFlag
	if (0 == (flags & 0x04))
	{
		//uint64_t startDecodingTimeStamp = 0; // file_reader_rb8(timeStampLength / 8)
		//uint64_t startCompositionTimeStamp = 0; // file_reader_rb8(timeStampLength / 8)
	}
	return file_reader_error(mov->fp);
}

static size_t mp4_write_sl_config_descriptor(const struct mov_t* mov)
{
	size_t size = 1;
	size += mov_write_base_descr(mov, ISO_SLConfigDescrTag, 1);
	file_writer_w8(mov->fp, 0x02);
	return size;
}

static int mp4_read_tag(struct mov_t* mov, uint64_t bytes)
{
	int tag, len;
	uint64_t p1, p2, offset;

	for (offset = 0; offset + 5 < bytes; offset += len)
	{
		tag = len = 0;
		offset += mov_read_base_descr(mov, &tag, &len);
		if (offset + len > bytes)
			break;

		p1 = file_reader_tell(mov->fp);
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

		p2 = file_reader_tell(mov->fp);
		file_reader_skip(mov->fp, len - (p2 - p1));
	}

	return file_reader_error(mov->fp);
}

// ISO/IEC 14496-14:2003(E) 5.6 Sample Description Boxes (p15)
int mov_read_esds(struct mov_t* mov, const struct mov_box_t* box)
{
	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	return mp4_read_tag(mov, box->size - 4);
}

static size_t mp4_write_es_descriptor(const struct mov_t* mov)
{
	size_t size = 3; // mp4_write_decoder_config_descriptor
	size += 5 + 13 + (mov->track->extra_data_size > 0 ? mov->track->extra_data_size + 5 : 0); // mp4_write_decoder_config_descriptor
	size += 5 + 1; // mp4_write_sl_config_descriptor

	size += mov_write_base_descr(mov, ISO_ESDescrTag, size);
	file_writer_wb16(mov->fp, (uint16_t)mov->track->tkhd.track_ID); // ES_ID
	file_writer_w8(mov->fp, 0x00); // flags (= no flags)

	mp4_write_decoder_config_descriptor(mov);
	mp4_write_sl_config_descriptor(mov);
	return size;
}

size_t mov_write_esds(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;

	size = 12 /* full box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "esds", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */

	size += mp4_write_es_descriptor(mov);

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}
