#include "mkv-buffer.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#define fseek64 _fseeki64
#define ftell64 _ftelli64
#elif defined(OS_LINUX)
#define fseek64 fseeko64
#define ftell64 ftello64
#else
#define fseek64 fseek
#define ftell64 ftell
#endif

static int mkv_file_read(void* fp, void* data, uint64_t bytes)
{
    if (bytes == fread(data, 1, bytes, (FILE*)fp))
        return 0;
	return 0 != ferror((FILE*)fp) ? ferror((FILE*)fp) : -1 /*EOF*/;
}

static int mkv_file_write(void* fp, const void* data, uint64_t bytes)
{
	return bytes == fwrite(data, 1, bytes, (FILE*)fp) ? 0 : ferror((FILE*)fp);
}

static int mkv_file_seek(void* fp, uint64_t offset)
{
	return fseek64((FILE*)fp, offset, SEEK_SET);
}

static uint64_t mkv_file_tell(void* fp)
{
	return ftell64((FILE*)fp);
}

const struct mkv_buffer_t* mkv_file_buffer(void)
{
	static struct mkv_buffer_t s_io = {
		mkv_file_read,
		mkv_file_write,
		mkv_file_seek,
		mkv_file_tell,
	};
	return &s_io;
}
