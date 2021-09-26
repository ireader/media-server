#ifndef _mov_ioutil_h_
#define _mov_ioutil_h_

#include "mov-buffer.h"

struct mov_ioutil_t
{
	struct mov_buffer_t io;
	void* param;
	int error;
};

static inline int mov_buffer_error(const struct mov_ioutil_t* io)
{
	return io->error;
}

static inline uint64_t mov_buffer_tell(const struct mov_ioutil_t* io)
{
	int64_t v;
	v = io->io.tell(io->param);
	if (v < 0)
		((struct mov_ioutil_t*)io)->error = -1;
	return v;
}

static inline void mov_buffer_seek(const struct mov_ioutil_t* io, int64_t offset)
{
//	if (0 == io->error)
		((struct mov_ioutil_t*)io)->error = io->io.seek(io->param, offset);
}

static inline void mov_buffer_skip(struct mov_ioutil_t* io, uint64_t bytes)
{
	uint64_t offset;
	if (0 == io->error)
	{
		offset = io->io.tell(io->param);
		io->error = io->io.seek(io->param, offset + bytes);
	}
}

static inline void mov_buffer_read(struct mov_ioutil_t* io, void* data, uint64_t bytes)
{
	if (0 == io->error)
		io->error = io->io.read(io->param, data, bytes);
}

static inline void mov_buffer_write(const struct mov_ioutil_t* io, const void* data, uint64_t bytes)
{
	if (0 == io->error)
		((struct mov_ioutil_t*)io)->error = io->io.write(io->param, data, bytes);
}

static inline uint8_t mov_buffer_r8(struct mov_ioutil_t* io)
{
	uint8_t v = 0;
	mov_buffer_read(io, &v, 1);
	return v;
}

static inline uint16_t mov_buffer_r16(struct mov_ioutil_t* io)
{
	uint16_t v;
	v = mov_buffer_r8(io);
	v = (v << 8) | mov_buffer_r8(io);
	return v;
}

static inline uint32_t mov_buffer_r24(struct mov_ioutil_t* io)
{
	uint32_t v;
	v = mov_buffer_r8(io);
	v = (v << 16) | mov_buffer_r16(io);
	return v;
}

static inline uint32_t mov_buffer_r32(struct mov_ioutil_t* io)
{
	uint32_t v;
	v = mov_buffer_r16(io);
	v = (v << 16) | mov_buffer_r16(io);
	return v;
}

static inline uint64_t mov_buffer_r64(struct mov_ioutil_t* io)
{
	uint64_t v;
	v = mov_buffer_r32(io);
	v = (v << 32) | mov_buffer_r32(io);
	return v;
}

static inline void mov_buffer_w8(const struct mov_ioutil_t* io, uint8_t v)
{
	mov_buffer_write(io, &v, 1);
}

static inline void mov_buffer_w16(const struct mov_ioutil_t* io, uint16_t v)
{
	mov_buffer_w8(io, (uint8_t)(v >> 8));
	mov_buffer_w8(io, (uint8_t)v);
}

static inline void mov_buffer_w24(const struct mov_ioutil_t* io, uint32_t v)
{
	mov_buffer_w16(io, (uint16_t)(v >> 8));
	mov_buffer_w8(io, (uint8_t)v);
}

static inline void mov_buffer_w32(const struct mov_ioutil_t* io, uint32_t v)
{
	mov_buffer_w16(io, (uint16_t)(v >> 16));
	mov_buffer_w16(io, (uint16_t)v);
}

static inline void mov_buffer_w64(const struct mov_ioutil_t* io, uint64_t v)
{
	mov_buffer_w32(io, (uint32_t)(v >> 32));
	mov_buffer_w32(io, (uint32_t)v);
}

#endif /* !_mov_ioutil_h_ */
