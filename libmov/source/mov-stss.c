#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

// 8.6.2 Sync Sample Box (p50)
int mov_read_stss(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_stbl_t* stbl = &mov->track->stbl;

	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	entry_count = file_reader_rb32(mov->fp);

	assert(0 == stbl->stss_count && NULL == stbl->stss);
	if (stbl->stss_count < entry_count)
	{
		void* p = realloc(stbl->stss, sizeof(stbl->stss[0]) * entry_count);
		if (NULL == p) return ENOMEM;
		stbl->stss = p;
	}
	stbl->stss_count = entry_count;

	for (i = 0; i < entry_count; i++)
		stbl->stss[i] = file_reader_rb32(mov->fp); // uint32_t sample_number

	return file_reader_error(mov->fp);
}
