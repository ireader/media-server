#include "mov-esds.h"
#include "file-reader.h"
#include "mov-internal.h"
#include <assert.h>

#define MP4ODescrTag                    0x01
#define MP4IODescrTag                   0x02
#define MP4ESDescrTag                   0x03
#define MP4DecConfigDescrTag            0x04
#define MP4DecSpecificDescrTag          0x05
#define MP4SLDescrTag                   0x06

static int mov_read_esds_descr(struct mov_reader_t* mov, int* tag,  int* len)
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

int mov_read_esds(struct mov_reader_t* mov, const struct mov_box_t* box)
{
	int tag, len;
	uint64_t pos, pos2;
	pos = file_reader_tell(mov->fp);

	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */

	tag = len = 0;
	mov_read_esds_descr(mov, &tag, &len);
	if (tag == MP4ESDescrTag)
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
	mov_read_esds_descr(mov, &tag, &len);
	if (tag == MP4DecConfigDescrTag)
	{
		int object_type_id = file_reader_r8(mov->fp);
		file_reader_r8(mov->fp); /* stream type */
		file_reader_rb24(mov->fp); /* buffer size db */

		uint32_t max_rate = file_reader_rb32(mov->fp); /* max bitrate */
		uint32_t bit_rate = file_reader_rb32(mov->fp);; /* avg bitrate */

		tag = len = 0;
		mov_read_esds_descr(mov, &tag, &len);
		if (tag == MP4DecSpecificDescrTag)
		{
			file_reader_seek(mov->fp, len);
		}
	}

	pos2 = file_reader_tell(mov->fp);
	if (pos2 - pos < box->size)
		file_reader_seek(mov->fp, box->size - (pos2 - pos));
	return 0;
}
