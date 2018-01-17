#include "mov-writer.h"
#include "mov-format.h"
#include "mpeg4-hevc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

#define H265_NAL(v)	((v >> 1) & 0x3F)

static uint8_t s_buffer[2 * 1024 * 1024];
static int s_width, s_height;

static uint8_t* file_read(const char* file, long* size)
{
	FILE* fp = fopen(file, "rb");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		*size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		uint8_t* ptr = (uint8_t*)malloc(*size);
		fread(ptr, 1, *size, fp);
		fclose(fp);

		return ptr;
	}

	return NULL;
}

static inline const uint8_t* h265_start_code(const uint8_t* ptr, const uint8_t* end)
{
	for (const uint8_t *p = ptr; p + 3 < end; p++)
	{
		if (0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00 == p[2] && 0x01 == p[3])))
			return p;
	}
	return end;
}

static inline int h265_nal_type(const unsigned char* ptr)
{
	int i = 2;
	assert(0x00 == ptr[0] && 0x00 == ptr[1]);
	if (0x00 == ptr[2])
		++i;
	assert(0x01 == ptr[i]);
	return H265_NAL(ptr[i + 1]);
}

static void h265_read_frame(mov_writer_t* mov, int width, int height, const uint8_t* ptr, const uint8_t* end)
{
	int64_t pts = 0;
	int track = -1;
	bool vpsspspps = false;
	struct mpeg4_hevc_t hevc;
	uint8_t extra_data[64 * 1024];

	memset(&hevc, 0, sizeof(hevc));
	const uint8_t* frame = h265_start_code(ptr, end);
	const uint8_t* nalu = frame;
	while (nalu < end)
	{
		const unsigned char* nalu2 = h265_start_code(nalu + 4, end);
		int nal_unit_type = h265_nal_type(nalu);
		if (nal_unit_type <= 31)
		{
			// process one frame
			size_t bytes = nalu2 - frame;
			assert(bytes < sizeof(s_buffer));
			size_t n = hevc_annexbtomp4(&hevc, frame, bytes, s_buffer, sizeof(s_buffer));
			frame = nalu2; // next frame

			if (!vpsspspps)
			{
				if (hevc.numOfArrays < 1)
					continue; // wait for key frame

				vpsspspps = true;
				int extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&hevc, extra_data, sizeof(extra_data));
				assert(extra_data_size > 0); // check buffer length
				track = mov_writer_add_video(mov, MOV_OBJECT_HEVC, width, height, extra_data, extra_data_size);
			}

			mov_writer_write(mov, track, s_buffer, n, pts, pts, (nal_unit_type >= 16 && nal_unit_type <= 23) ? MOV_AV_FLAG_KEYFREAME : 0);
			pts += 40;
		}

		nalu = nalu2;
	}
}

void mov_writer_h265(const char* h265, int width, int height, const char* mp4)
{
	long bytes = 0;
	uint8_t* ptr = file_read(h265, &bytes);
	if (NULL == ptr) return;

	FILE* fp = fopen(mp4, "wb+");
	mov_writer_t* mov = mov_writer_create(mov_file_buffer(), fp, MOV_FLAG_FASTSTART);
	h265_read_frame(mov, width, height, ptr, ptr + bytes);
	mov_writer_destroy(mov);

	fclose(fp);
	free(ptr);
}
