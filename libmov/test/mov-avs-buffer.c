#if defined(__AVS__)

#include "mov-buffer.h"
#include "mov-avs-buffer.h"
#include "avs-file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int mov_avs_read(void* fp, void* data, uint64_t bytes)
{
	return avs_file_read((struct avs_file_t*)fp, data, bytes);
}

static int mov_avs_write(void* fp, const void* data, uint64_t bytes)
{
	return -1;
}

static int mov_avs_seek(void* fp, int64_t offset)
{
	return avs_file_seek((struct avs_file_t*)fp, offset, offset >= 0 ? AVS_SEEK_SET : AVS_SEEK_END);
}

static int64_t mov_avs_tell(void* fp)
{
	return avs_file_tell((struct avs_file_t*)fp);
}

const struct mov_buffer_t* mov_avs_buffer(void)
{
	static struct mov_buffer_t s_io = {
		mov_avs_read,
		mov_avs_write,
		mov_avs_seek,
		mov_avs_tell,
	};
	return &s_io;
}

#endif /* __AVS__*/
