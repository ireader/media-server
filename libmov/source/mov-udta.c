#include "mov-udta.h"
#include "mov-ioutil.h"
#include "mov-memory-buffer.h"
#include "mov-internal.h"

int mov_read_udta(struct mov_t* mov, const struct mov_box_t* box)
{
	mov_buffer_skip(&mov->io, box->size);
	return mov_buffer_error(&mov->io);
}

size_t mov_write_udta(const struct mov_t* mov)
{
	if (!mov->udta || mov->udta_size < 1)
		return 0;

	mov_buffer_w32(&mov->io, 8 + (uint32_t)mov->udta_size);
	mov_buffer_write(&mov->io, "udta", 4);
	mov_buffer_write(&mov->io, mov->udta, mov->udta_size);
	return 8 + (size_t)mov->udta_size;
}

int mov_udta_meta_write(const struct mov_udta_meta_t* meta, void* data, int bytes)
{
	struct mov_ioutil_t w;
	struct mov_memory_buffer_t ptr;
	uint64_t pmeta, pilst, n;

	ptr.bytes = 0;
	ptr.maxsize = bytes;
	ptr.capacity = bytes;
	ptr.off = 0;
	ptr.ptr = (uint8_t*)data;
	memset(&w, 0, sizeof(w));
	memcpy(&w.io, mov_memory_buffer(), sizeof(w.io));
	w.param = &ptr;
	
	pmeta = mov_buffer_tell(&w);
	mov_buffer_w32(&w, 0); // placeholder
	mov_buffer_write(&w, "meta", 4);
	mov_buffer_w32(&w, 0); /* version & flags */

	mov_buffer_w32(&w, 33);
	mov_buffer_write(&w, "hdlr", 4);
	mov_buffer_w32(&w, 0); /* version & flags */
	mov_buffer_w32(&w, 0);
	mov_buffer_write(&w, "mdir", 4);
	mov_buffer_write(&w, "appl", 4);
	mov_buffer_w32(&w, 0);
	mov_buffer_w32(&w, 0);
	mov_buffer_w8(&w, 0);

	pilst = mov_buffer_tell(&w);
	mov_buffer_w32(&w, 0); // placeholder
	mov_buffer_write(&w, "ilst", 4);

	// write cover
	mov_buffer_w32(&w, meta->cover_size + 16 + 8);
	mov_buffer_write(&w, "covr", 4);
	mov_buffer_w32(&w, meta->cover_size + 16);
	mov_buffer_write(&w, "data", 4);
	mov_buffer_w32(&w, 0); // TODO track tag
	mov_buffer_w32(&w, 0);
	mov_buffer_write(&w, meta->cover, meta->cover_size);

	// update box size
	n = mov_buffer_tell(&w);
	mov_buffer_seek(&w, pilst);
	mov_buffer_w32(&w, (uint32_t)(n - pilst));
	mov_buffer_seek(&w, pmeta);
	mov_buffer_w32(&w, (uint32_t)(n - pmeta));
	mov_buffer_seek(&w, n); // rewind

	return (int)ptr.bytes;
}

int mov_udta_chapter_write(const struct mov_udta_chapter_t* chapters, int count, void* data, int bytes)
{
	struct mov_ioutil_t w;
	struct mov_memory_buffer_t ptr;
	uint64_t pmeta, pilst, pchpl, n;
	int i;
	static const uint8_t ilst[] = { 0x00, 0x00, 0x00, 0x25, 0xa9, 0x74, 0x6f, 0x6f, 0x00, 0x00, 0x00, 0x1d, 0x64, 0x61, 0x74, 0x61, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x4c, 0x61, 0x76, 0x66, 0x36, 0x30, 0x2e, 0x32, 0x30, 0x2e, 0x31, 0x30, 0x30 };

	ptr.bytes = 0;
	ptr.maxsize = bytes;
	ptr.capacity = bytes;
	ptr.off = 0;
	ptr.ptr = (uint8_t*)data;
	memset(&w, 0, sizeof(w));
	memcpy(&w.io, mov_memory_buffer(), sizeof(w.io));
	w.param = &ptr;

	pmeta = mov_buffer_tell(&w);
	mov_buffer_w32(&w, 0); // placeholder
	mov_buffer_write(&w, "meta", 4);
	mov_buffer_w32(&w, 0); /* version & flags */

	mov_buffer_w32(&w, 33);
	mov_buffer_write(&w, "hdlr", 4);
	mov_buffer_w32(&w, 0); /* version & flags */
	mov_buffer_w32(&w, 0);
	mov_buffer_write(&w, "mdir", 4);
	mov_buffer_write(&w, "appl", 4);
	mov_buffer_w32(&w, 0);
	mov_buffer_w32(&w, 0);
	mov_buffer_w8(&w, 0);

	mov_buffer_w32(&w, 0x2d); // placeholder
	mov_buffer_write(&w, "ilst", 4);
	mov_buffer_write(&w, ilst, sizeof(ilst));

	// update meta box size
	n = mov_buffer_tell(&w);
	mov_buffer_seek(&w, pmeta);
	mov_buffer_w32(&w, (uint32_t)(n - pmeta));
	mov_buffer_seek(&w, n); // rewind

	pchpl = mov_buffer_tell(&w);
	mov_buffer_w32(&w, 0); // placeholder
	mov_buffer_write(&w, "chpl", 4);
	mov_buffer_w8(&w, 1); /* version */
	mov_buffer_w24(&w, 0); /* flags */
	mov_buffer_w32(&w, 0); /* unknown*/

	mov_buffer_w8(&w, (uint8_t)count);
	for (i = 0; i < count; i++)
	{
		mov_buffer_w64(&w, chapters[i].timestamp);
		if (chapters[i].title && *chapters[i].title && strlen(chapters[i].title) <= 255)
		{
			mov_buffer_w8(&w, (uint8_t)strlen(chapters[i].title));
			mov_buffer_write(&w, chapters[i].title, strlen(chapters[i].title));
		}
		else
		{
			mov_buffer_w8(&w, 0);
		}
	}

	// update chpl box size
	n = mov_buffer_tell(&w);
	mov_buffer_seek(&w, pchpl);
	mov_buffer_w32(&w, (uint32_t)(n - pchpl));
	mov_buffer_seek(&w, n); // rewind

	return (int)ptr.bytes;
}
