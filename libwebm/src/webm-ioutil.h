#ifndef _webm_ioutil_h_
#define _webm_ioutil_h_

#include "webm-buffer.h"
#include "stdint.h"

struct webm_ioutil_t
{
	struct webm_buffer_t io;
	void* param;
	int error;
};

static inline int webm_buffer_error(const struct webm_ioutil_t* io)
{
	return io->error;
}

static inline uint64_t webm_buffer_tell(const struct webm_ioutil_t* io)
{
	return io->io.tell(io->param);
}

static inline void webm_buffer_seek(const struct webm_ioutil_t* io, uint64_t offset)
{
	//	if (0 == io->error)
	((struct webm_ioutil_t*)io)->error = io->io.seek(io->param, offset);
}

static inline void webm_buffer_skip(struct webm_ioutil_t* io, uint64_t bytes)
{
	uint64_t offset;
	if (0 == io->error)
	{
		offset = io->io.tell(io->param);
		io->error = io->io.seek(io->param, offset + bytes);
	}
}

static inline void webm_buffer_read(struct webm_ioutil_t* io, void* data, uint64_t bytes)
{
	if (0 == io->error)
		io->error = io->io.read(io->param, data, bytes);
}

static inline void webm_buffer_write(const struct webm_ioutil_t* io, const void* data, uint64_t bytes)
{
	if (0 == io->error)
		((struct webm_ioutil_t*)io)->error = io->io.write(io->param, data, bytes);
}

uint32_t webm_buffer_read_id(struct webm_ioutil_t* io);

uint64_t webm_buffer_read_size(struct webm_ioutil_t* io);

static inline uint8_t webm_buffer_r8(struct webm_ioutil_t* io)
{
	uint8_t v = 0;
	webm_buffer_read(io, &v, 1);
	return v;
}

static inline void webm_buffer_w8(const struct webm_ioutil_t* io, uint8_t v)
{
	webm_buffer_write(io, &v, 1);
}

static inline uint64_t webm_buffer_read_uint(struct webm_ioutil_t* io, int n)
{
	uint64_t v;
	v = 0;
	while (n-- > 0)
	{
		v = (v << 8) | webm_buffer_r8(io);
	}
	return v;
}

static inline int64_t webm_buffer_read_int(struct webm_ioutil_t* io, int n)
{
	int64_t v;
	v = (int64_t)webm_buffer_read_uint(io, n);

	// sign_extend
	return n < 1 ? 0 : ( (v & (1LL<<((n*8)-1))) ? v | (0xFFFFFFFFFFFFFFFFULL << (n*8)) : v);
}

static inline double webm_buffer_read_float(struct webm_ioutil_t* io, int n)
{
	uint64_t v;

	switch (n)
	{
	case 0:
		return 0.0;

	case 4:
		v = webm_buffer_read_uint(io, n);
		return (float)(uint32_t)v;

	case 8:
		v = webm_buffer_read_uint(io, n);
		return (double)v;

	default:
		assert(0);
		io->error = -1;
		return 0.0;
	}
}

static inline void webm_buffer_write_int(const struct webm_ioutil_t* io, int64_t v)
{
	webm_buffer_write(io, &v, 1);
}

static inline void webm_buffer_write_uint(const struct webm_ioutil_t* io, uint64_t v)
{
	webm_buffer_w8(io, (uint8_t)(v >> 8));
	webm_buffer_w8(io, (uint8_t)v);
}

#endif /* !_webm_ioutil_h_ */

