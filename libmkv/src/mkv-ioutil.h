#ifndef _mkv_ioutil_h_
#define _mkv_ioutil_h_

#include "mkv-buffer.h"
#include "ebml.h"
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
	int64_t v;
	v = io->io.tell(io->param);
	if (v < 0)
		((struct mkv_ioutil_t*)io)->error = -1;
	return v;
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

void mkv_buffer_write_signed_size(struct mkv_ioutil_t* io, int64_t size);

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

static inline void mkv_buffer_w16(const struct mkv_ioutil_t* io, uint16_t v)
{
	uint8_t w16[2];
	w16[0] = (uint8_t)(v >> 8);
	w16[1] = (uint8_t)v;
	mkv_buffer_write(io, w16, 2);
}

static inline void mkv_buffer_w32(const struct mkv_ioutil_t* io, uint32_t v)
{
	uint8_t w32[4];
	w32[0] = (uint8_t)(v >> 24);
	w32[1] = (uint8_t)(v >> 16);
	w32[2] = (uint8_t)(v >> 8);
	w32[3] = (uint8_t)v;
	mkv_buffer_write(io, w32, 4);
}

static inline void mkv_buffer_w64(const struct mkv_ioutil_t* io, uint64_t v)
{
	uint8_t w64[8];
	w64[0] = (uint8_t)(v >> 56);
	w64[1] = (uint8_t)(v >> 48);
	w64[2] = (uint8_t)(v >> 40);
	w64[3] = (uint8_t)(v >> 32);
	w64[4] = (uint8_t)(v >> 24);
	w64[5] = (uint8_t)(v >> 16);
	w64[6] = (uint8_t)(v >> 8);
	w64[7] = (uint8_t)v;
	mkv_buffer_write(io, w64, 8);
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

static inline void mkv_buffer_write_master(struct mkv_ioutil_t* io, uint32_t id, uint64_t size, unsigned int bytes)
{
	unsigned int n;
	uint8_t buf[4 + 8];
	n = ebml_write_element(buf, id, size, bytes);
	mkv_buffer_write(io, buf, n);
}

static inline void mkv_buffer_write_uint_element(struct mkv_ioutil_t* io, uint32_t id, uint64_t v)
{
	unsigned int n;
	uint8_t buf[8];
	mkv_buffer_write_master(io, id, ebml_uint_length(v), 0);

	n = ebml_write_uint(buf, v);
	mkv_buffer_write(io, buf, n);
}

//static inline void mkv_buffer_write_int_element(struct mkv_ioutil_t* io, uint32_t id, int64_t v)
//{
//	uint8_t buf[4 + 8 + 8];
//    uint64_t tmp = 2*(v < 0 ? v^-1 : v);
//	mkv_buffer_write_master(io, id, ebml_uint_length(tmp), 0);
//    
//    do
//    {
//        mkv_buffer_w8(io, (uint8_t)v);
//        v >>= 8;
//    } while(v > 0);
//}

static inline void mkv_buffer_write_double_element(struct mkv_ioutil_t* io, uint32_t id, double v)
{
	mkv_buffer_write_master(io, id, 8, 0);
	mkv_buffer_w64(io, *(uint64_t*)&v);
}

static inline void mkv_buffer_write_string_element(struct mkv_ioutil_t* io, uint32_t id, const char* s, size_t n)
{
	mkv_buffer_write_master(io, id, n, 0);
    mkv_buffer_write(io, s, n);
}

static inline void mkv_buffer_write_binary_element(struct mkv_ioutil_t* io, uint32_t id, const void* s, size_t n)
{
	mkv_buffer_write_string_element(io, id, (const char*)s, n);
}

#endif /* !_mkv_ioutil_h_ */
