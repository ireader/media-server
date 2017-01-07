#include "file-reader.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#define FILE_CAPACITY (2*1024)

struct file_t
{
	FILE* fp;
	int error;

	uint64_t offset;
	uint64_t bytes;
	uint8_t* ptr;
};

void* file_reader_create(const char* file)
{
	struct file_t* f;
	f = (struct file_t*)malloc(sizeof(*f) + FILE_CAPACITY);
	if (NULL == f)
		return NULL;

	memset(f, 0, sizeof(*f));
	f->ptr = (uint8_t*)(f + 1);
	f->bytes = 0;
	f->offset = 0;
	f->fp = fopen(file, "rb");
	if (NULL == f->fp)
	{
		file_reader_destroy(&f);
		return NULL;
	}

	return f;
}

void file_reader_destroy(void** file)
{
	struct file_t* f;
	if(NULL == file || NULL == *file)
		return;

	f = *(struct file_t**)file;
	if(f->fp)
		fclose(f->fp);
	free(f);
	*file = NULL;
}

int file_reader_error(void* file)
{
	struct file_t* f = (struct file_t*)file;
	return f->error;
}

int file_reader_seek(void* file, uint64_t bytes)
{
	struct file_t* f = (struct file_t*)file;
	f->offset += bytes;
	if (f->offset > f->bytes)
	{
		while (f->offset - f->bytes > INT32_MAX)
		{
			f->error = fseek(f->fp, INT32_MAX, SEEK_CUR);
			f->offset -= INT32_MAX;
		}
		f->error = fseek(f->fp, f->offset - f->bytes, SEEK_CUR);
		f->bytes = f->offset = 0; // clear buffer offset/size
	}
	return f->error;
}

uint64_t file_reader_tell(void* file)
{
	long n;
	struct file_t* f = (struct file_t*)file;
	n = ftell(f->fp);
	if (f->offset < f->bytes)
		n -= f->bytes - f->offset;
	return n;
}

size_t file_reader_read(void* file, void* buffer, size_t bytes)
{
	size_t n, r = 0;
	struct file_t* f = (struct file_t*)file;

	// copy from memory
	if(bytes > r && f->offset < f->bytes) 
	{
		r = bytes <= (f->bytes - f->offset) ? bytes : (f->bytes - f->offset);
		memcpy(buffer, f->ptr + f->offset, r);
		f->offset += r;
	}

	// load from file
	n = bytes - r;
	if(n > 0) 
	{
		if(n >= FILE_CAPACITY) 
		{
			r += fread((uint8_t*)buffer + r, 1, n, f->fp);
		} 
		else 
		{
			f->bytes = fread(f->ptr, 1, FILE_CAPACITY, f->fp);
			if (0 == f->bytes)
			{
				f->error = -1; // end-of-file mark an error
				return 0;
			}

			f->offset = f->bytes >= n ? n : f->bytes;
			memcpy((uint8_t*)buffer + r, f->ptr, f->offset);
			r += f->offset;
		}
	}

	return r;
}

unsigned int file_reader_r8(void* file)
{
	struct file_t* f = (struct file_t*)file;
	if (f->offset >= f->bytes)
	{
		f->offset = 0;
		f->bytes = fread(f->ptr, 1, FILE_CAPACITY, f->fp);
		if (0 == f->bytes)
			f->error = -1; // end-of-file mark an error
	}

	if (f->offset < f->bytes)
		return f->ptr[f->offset++];

	assert(0 != f->error);
	return 0;
}

unsigned int file_reader_rl16(void* file)
{
	unsigned int val;
	val = file_reader_r8(file);
	val |= file_reader_r8(file) << 8;
	return val;
}

unsigned int file_reader_rl24(void* file)
{
	unsigned int val;
	val = file_reader_rl16(file);
	val |= file_reader_r8(file) << 16;
	return val;
}

unsigned int file_reader_rl32(void* file)
{
	unsigned int val;
	val = file_reader_rl16(file);
	val |= file_reader_rl16(file) << 16;
	return val;
}

uint64_t file_reader_rl64(void* file)
{
	uint64_t val;
	val = file_reader_rl32(file);
	val |= (uint64_t)file_reader_rl32(file) << 32;
	return val;
}

unsigned int file_reader_rb16(void* file)
{
	unsigned int val;
	val = file_reader_r8(file) << 8;
	val |= file_reader_r8(file);
	return val;
}

unsigned int file_reader_rb24(void* file)
{
	unsigned int val;
	val = file_reader_rb16(file) << 16;
	val |= file_reader_r8(file);
	return val;
}

unsigned int file_reader_rb32(void* file)
{
	unsigned int val;
	val = file_reader_rb16(file) << 16;
	val |= file_reader_rb16(file);
	return val;
}

uint64_t file_reader_rb64(void* file)
{
	uint64_t val;
	val = (uint64_t)file_reader_rb32(file) << 32;
	val |= (uint64_t)file_reader_rb32(file);
	return val;
}
