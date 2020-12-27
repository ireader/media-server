#ifndef _mkv_ioutil_h_
#define _mkv_ioutil_h_

#include "mkv-buffer.h"
#include <stdint.h>
#include <assert.h>

struct mkv_ioutil_t
{
	struct mkv_buffer_t io;
	void* param;
	int error;
};

static inline int mkv_buffer_error(const struct mkv_ioutil_t* io)
{
	return io->error;
}

static inline uint64_t mkv_buffer_tell(const struct mkv_ioutil_t* io)
{
	return io->io.tell(io->param);
}

static inline void mkv_buffer_seek(const struct mkv_ioutil_t* io, uint64_t offset)
{
	//	if (0 == io->error)
	((struct mkv_ioutil_t*)io)->error = io->io.seek(io->param, offset);
}

static inline void mkv_buffer_skip(struct mkv_ioutil_t* io, uint64_t bytes)
{
	uint64_t offset;
	if (0 == io->error)
	{
		offset = io->io.tell(io->param);
		io->error = io->io.seek(io->param, offset + bytes);
	}
}

static inline void mkv_buffer_read(struct mkv_ioutil_t* io, void* data, uint64_t bytes)
{
	if (0 == io->error)
		io->error = io->io.read(io->param, data, bytes);
}

static inline void mkv_buffer_write(const struct mkv_ioutil_t* io, const void* data, uint64_t bytes)
{
	if (0 == io->error)
		((struct mkv_ioutil_t*)io)->error = io->io.write(io->param, data, bytes);
}

uint32_t mkv_buffer_read_id(struct mkv_ioutil_t* io);

int64_t mkv_buffer_read_size(struct mkv_ioutil_t* io);

int64_t mkv_buffer_read_signed_size(struct mkv_ioutil_t* io);

static inline uint8_t mkv_buffer_r8(struct mkv_ioutil_t* io)
{
	uint8_t v = 0;
	mkv_buffer_read(io, &v, 1);
	return v;
}

static inline void mkv_buffer_w8(const struct mkv_ioutil_t* io, uint8_t v)
{
	mkv_buffer_write(io, &v, 1);
}

static inline uint64_t mkv_buffer_read_uint(struct mkv_ioutil_t* io, int n)
{
	uint64_t v;
	v = 0;
	while (n-- > 0)
	{
		v = (v << 8) | mkv_buffer_r8(io);
	}
	return v;
}

static inline int64_t mkv_buffer_read_int(struct mkv_ioutil_t* io, int n)
{
	int64_t v;
	v = (int64_t)mkv_buffer_read_uint(io, n);

	// sign_extend
	return n < 1 ? 0 : ( (v & (1LL<<((n*8)-1))) ? v | (0xFFFFFFFFFFFFFFFFULL << (n*8)) : v);
}

static inline double mkv_buffer_read_float(struct mkv_ioutil_t* io, int n)
{
	uint64_t v;

	switch (n)
	{
	case 0:
		return 0.0;

	case 4:
		v = mkv_buffer_read_uint(io, n);
		return (float)(uint32_t)v;

	case 8:
		v = mkv_buffer_read_uint(io, n);
		return (double)v;

	default:
		assert(0);
		io->error = -1;
		return 0.0;
	}
}

static inline void mkv_buffer_write_int(const struct mkv_ioutil_t* io, int64_t v)
{
	mkv_buffer_write(io, &v, 1);
}

static inline void mkv_buffer_write_uint(const struct mkv_ioutil_t* io, uint64_t v)
{
	mkv_buffer_w8(io, (uint8_t)(v >> 8));
	mkv_buffer_w8(io, (uint8_t)v);
}

#endif /* !_mkv_ioutil_h_ */

