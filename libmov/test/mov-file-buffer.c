#include "mov-buffer.h"
#include "mov-file-buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

static int mov_file_read(void* fp, void* data, uint64_t bytes)
{
    if (bytes == fread(data, 1, bytes, (FILE*)fp))
        return 0;
	return 0 != ferror((FILE*)fp) ? ferror((FILE*)fp) : -1 /*EOF*/;
}

static int mov_file_write(void* fp, const void* data, uint64_t bytes)
{
	return bytes == fwrite(data, 1, bytes, (FILE*)fp) ? 0 : ferror((FILE*)fp);
}

static int mov_file_seek(void* fp, int64_t offset)
{
	return fseek64((FILE*)fp, offset, offset >= 0 ? SEEK_SET : SEEK_END);
}

static int64_t mov_file_tell(void* fp)
{
	return ftell64((FILE*)fp);
}

static int mov_file_cache_read(void* fp, void* data, uint64_t bytes)
{
	uint8_t* p = (uint8_t*)data;
	struct mov_file_cache_t* file = (struct mov_file_cache_t*)fp;
	while (bytes > 0)
	{
		assert(file->off <= file->len);
		if (file->off >= file->len)
		{
			if (bytes >= sizeof(file->ptr))
			{
				if (bytes == fread(p, 1, bytes, file->fp))
				{
					file->tell += bytes;
					return 0;
				}
				return 0 != ferror(file->fp) ? ferror(file->fp) : -1 /*EOF*/;
			}
			else
			{
				file->off = 0;
				file->len = fread(file->ptr, 1, (int)sizeof(file->ptr), file->fp);
				if (file->len < 1)
					return 0 != ferror(file->fp) ? ferror(file->fp) : -1 /*EOF*/;
			}
		}

		if (file->off < file->len)
		{
			unsigned int n = file->len - file->off;
			n = n > bytes ? bytes : n;
			memcpy(p, file->ptr + file->off, n);
			file->tell += n;
			file->off += n;
			bytes -= n;
			p += n;
		}
	}

	return 0;
}

static int mov_file_cache_write(void* fp, const void* data, uint64_t bytes)
{
	struct mov_file_cache_t* file = (struct mov_file_cache_t*)fp;
	
	file->tell += bytes;

	if (file->off + bytes < sizeof(file->ptr))
	{
		memcpy(file->ptr + file->off, data, bytes);
		file->off += bytes;
		return 0;
	}

	// write buffer
	if (file->off > 0)
	{
		if (file->off != fwrite(file->ptr, 1, file->off, file->fp))
			return ferror(file->fp);
		file->off = 0; // clear buffer
	}

	// write data;
	return bytes == fwrite(data, 1, bytes, file->fp) ? 0 : ferror(file->fp);
}

static int mov_file_cache_seek(void* fp, int64_t offset)
{
	int r;
	struct mov_file_cache_t* file = (struct mov_file_cache_t*)fp;
	if (offset != file->tell)
	{
		if (file->off > file->len)
		{
			// write bufferred data
			if(file->off != fwrite(file->ptr, 1, file->off, file->fp))
				return ferror(file->fp);
		}

		file->off = file->len = 0;
		r = fseek64(file->fp, offset, offset >= 0 ? SEEK_SET : SEEK_END);
		file->tell = ftell64(file->fp);
		return r;
	}
	return 0;
}

static int64_t mov_file_cache_tell(void* fp)
{
	struct mov_file_cache_t* file = (struct mov_file_cache_t*)fp;
	if (ftell64(file->fp) != file->tell + (int)(file->len - file->off))
		return -1;
	return (int64_t)file->tell;
	//return ftell64(file->fp);
}

const struct mov_buffer_t* mov_file_buffer(void)
{
	static struct mov_buffer_t s_io = {
		mov_file_read,
		mov_file_write,
		mov_file_seek,
		mov_file_tell,
	};
	return &s_io;
}

const struct mov_buffer_t* mov_file_cache_buffer(void)
{
	static struct mov_buffer_t s_io = {
		mov_file_cache_read,
		mov_file_cache_write,
		mov_file_cache_seek,
		mov_file_cache_tell,
	};
	return &s_io;
}
