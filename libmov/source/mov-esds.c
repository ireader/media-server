#include "mov-esds.h"
#include "file-reader.h"
#include "mov-internal.h"
#include "mov-tag.h"
#include <assert.h>

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

// ISO/IEC 14496-1:2010(E) 7.2.6.6 DecoderConfigDescriptor (p48)
/*
class DecoderConfigDescriptor extends BaseDescriptor : bit(8)
tag=DecoderConfigDescrTag {
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

// ISO/IEC 14496-14:2003(E) 5.6 Sample Description Boxes (p15)
int mov_read_esds(struct mov_t* mov, const struct mov_box_t* box)
{
	int tag, len;
	uint64_t pos, pos2;
	pos = file_reader_tell(mov->fp);

	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */

	tag = len = 0;
	mov_read_base_descr(mov, &tag, &len);
	if (tag == ISO_ESDescrTag)
	{
		uint32_t ES_ID = file_reader_rb16(mov->fp);
		uint32_t flags = file_reader_r8(mov->fp);
		if (flags & 0x80) //streamDependenceFlag
			file_reader_rb16(mov->fp);
		if (flags & 0x40) { //URL_Flag
			uint32_t n = file_reader_r8(mov->fp);
			file_reader_seek(mov->fp, n);
		}
		if (flags & 0x20) //OCRstreamFlag
			file_reader_rb16(mov->fp);
	}
	else
	{
		file_reader_seek(mov->fp, 2); /* ID */
	}

	tag = len = 0;
	mov_read_base_descr(mov, &tag, &len);
	if (tag == ISO_DecoderConfigDescrTag)
	{
		int object_type_id = file_reader_r8(mov->fp);
		file_reader_r8(mov->fp); /* stream type */
		file_reader_rb24(mov->fp); /* buffer size db */

		uint32_t max_rate = file_reader_rb32(mov->fp); /* max bit-rate */
		uint32_t bit_rate = file_reader_rb32(mov->fp);; /* avg bit-rate */

		tag = len = 0;
		mov_read_base_descr(mov, &tag, &len);
		if (tag == ISO_DecSpecificInfoTag)
		{
			file_reader_seek(mov->fp, len);
		}
	}

	pos2 = file_reader_tell(mov->fp);
	if (pos2 - pos < box->size)
		file_reader_seek(mov->fp, box->size - (pos2 - pos));
	return 0;
}
