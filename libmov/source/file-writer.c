#include "file-writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#define FILE_CACHE (2*1024)

struct wfile_t
{
	FILE* fp;
	int error;

	size_t bytes;
	uint8_t* ptr;
};

void* file_writer_create(const char* file)
{
	struct wfile_t* f;
	f = (struct wfile_t*)malloc(sizeof(*f) + FILE_CACHE);
	if (NULL == f)
		return NULL;

	memset(f, 0, sizeof(*f));
	f->ptr = (uint8_t*)(f + 1);
	f->bytes = 0;
	f->fp = fopen(file, "wb");
	if (NULL == f->fp)
	{
		file_writer_destroy(&f);
		return NULL;
	}

	return f;
};

static void file_writer_flush(struct wfile_t* f)
{
	size_t r;
	if (f->bytes > 0)
	{
		r = fwrite(f->ptr, 1, f->bytes, f->fp);
		if (r != f->bytes)
			f->error = ferror(f->fp);
		f->bytes -= r;
	}
}

void file_writer_destroy(void** file)
{
	struct wfile_t* f;
	if(NULL == file || NULL == *file)
		return;

	f = *(struct wfile_t**)file;
	if (f->fp)
	{
		file_writer_flush(f);
		fclose(f->fp);
	}

	free(f);
	*file = NULL;
}

/// @return 0-ok/eof, other-error
int file_writer_error(void* file)
{
	struct wfile_t* f = (struct wfile_t*)file;
	return f->error;
}

int file_writer_seek(void* file, uint64_t offset)
{
	struct wfile_t* f = (struct wfile_t*)file;
	file_writer_flush(f);
	return fseek(f->fp, offset, SEEK_SET);
}

uint64_t file_writer_tell(void* file)
{
	long n;
	struct wfile_t* f = (struct wfile_t*)file;
	n = ftell(f->fp);
	if(-1L == n)
	{
		f->error = errno;
		return 0;
	}
	return n + f->bytes;
}

/// @return 0-if error, other-if don't return bytes, check error with file_reader_error
size_t file_writer_write(void* file, const void* buffer, size_t bytes)
{
	struct wfile_t* f = (struct wfile_t*)file;
	if (bytes + f->bytes <= FILE_CACHE)
	{
		memcpy(f->ptr + f->bytes, buffer, bytes);
		f->bytes += bytes;
	}
	else
	{
		file_writer_flush(f);
		assert(0 == f->bytes);

		if (bytes + f->bytes <= FILE_CACHE)
		{
			memcpy(f->ptr + f->bytes, buffer, bytes);
			f->bytes += bytes;
		}
		else
		{
			if (bytes != fwrite(buffer, 1, bytes, f->fp))
			{
				f->error = ferror(f->fp);
				return 0;
			}
		}
	}

	return bytes;
}

void file_writer_w8(void* file, uint8_t value)
{
	file_writer_write(file, &value, 1);
}

// little-endian
void file_writer_wl16(void* file, uint16_t value)
{
	file_writer_w8(file, (uint8_t)(value & 0xFF));
	file_writer_w8(file, (uint8_t)((value >> 8) & 0xFF));
}

void file_writer_wl24(void* file, uint32_t value)
{
	file_writer_wl16(file, (uint16_t)value);
	file_writer_w8(file, (uint8_t)((value >> 16) & 0xFF));
}

void file_writer_wl32(void* file, uint32_t value)
{
	file_writer_wl16(file, (uint16_t)value);
	file_writer_wl16(file, (uint16_t)((value >> 16) & 0xFFFF));
}

void file_writer_wl64(void* file, uint64_t value)
{
	file_writer_wl32(file, (uint32_t)value);
	file_writer_wl32(file, (uint32_t)((value >> 32) & 0xFFFFFFFF));
}

// big-endian
void file_writer_wb16(void* file, uint16_t value)
{
	file_writer_w8(file, (uint8_t)((value >> 8) & 0xFF));
	file_writer_w8(file, (uint8_t)(value & 0xFF));	
}

void file_writer_wb24(void* file, uint32_t value)
{
	file_writer_w8(file, (uint8_t)((value >> 16) & 0xFF));
	file_writer_wb16(file, (uint16_t)value);	
}

void file_writer_wb32(void* file, uint32_t value)
{
	file_writer_wb16(file, (uint16_t)((value >> 16) & 0xFFFF));
	file_writer_wb16(file, (uint16_t)value);
}

void file_writer_wb64(void* file, uint64_t value)
{
	file_writer_wl32(file, (uint32_t)((value >> 32) & 0xFFFFFFFF));
	file_writer_wl32(file, (uint32_t)value);	
}
