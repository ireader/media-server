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

uint64_t webm_buffer_r64(struct webm_ioutil_t* io);

static inline uint8_t webm_buffer_r8(struct webm_ioutil_t* io)
{
	return (uint8_t)webm_buffer_r64(io);
}

static inline uint16_t webm_buffer_r16(struct webm_ioutil_t* io)
{
	return (uint16_t)webm_buffer_r64(io);
}

static inline uint32_t webm_buffer_r24(struct webm_ioutil_t* io)
{
	return (uint32_t)webm_buffer_r64(io);
}

static inline uint32_t webm_buffer_r32(struct webm_ioutil_t* io)
{
	return (uint32_t)webm_buffer_r64(io);
}

static inline void webm_buffer_w8(const struct webm_ioutil_t* io, uint8_t v)
{
	webm_buffer_write(io, &v, 1);
}

static inline void webm_buffer_w16(const struct webm_ioutil_t* io, uint16_t v)
{
	webm_buffer_w8(io, (uint8_t)(v >> 8));
	webm_buffer_w8(io, (uint8_t)v);
}

static inline void webm_buffer_w24(const struct webm_ioutil_t* io, uint32_t v)
{
	webm_buffer_w16(io, (uint16_t)(v >> 8));
	webm_buffer_w8(io, (uint8_t)v);
}

static inline void webm_buffer_w32(const struct webm_ioutil_t* io, uint32_t v)
{
	webm_buffer_w16(io, (uint16_t)(v >> 16));
	webm_buffer_w16(io, (uint16_t)v);
}

static inline void webm_buffer_w64(const struct webm_ioutil_t* io, uint64_t v)
{
	webm_buffer_w32(io, (uint32_t)(v >> 32));
	webm_buffer_w32(io, (uint32_t)v);
}

#endif /* !_webm_ioutil_h_ */

