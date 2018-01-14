#include "mov-writer.h"
#include "mov-format.h"
#include "mpeg4-avc.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

#define H264_NAL(v)	(v & 0x1F)

enum { NAL_IDR = 5, NAL_SEI = 6, NAL_SPS = 7, NAL_PPS = 8 };

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

static inline const uint8_t* h264_start_code(const uint8_t* ptr, const uint8_t* end)
{
	for (const uint8_t *p = ptr; p + 3 < end; p++)
	{
		if (0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00 == p[2] && 0x01 == p[3])))
			return p;
	}
	return end;
}

static inline int h264_nal_type(const unsigned char* ptr)
{
	int i = 2;
	assert(0x00 == ptr[0] && 0x00 == ptr[1]);
	if (0x00 == ptr[2])
		++i;
	assert(0x01 == ptr[i]);
	return H264_NAL(ptr[i + 1]);
}

static void h264_read_frame(mov_writer_t* mov, int width, int height, const uint8_t* ptr, const uint8_t* end)
{
	int64_t pts = 0;
	int track = -1;
	bool spspps = false;
	struct mpeg4_avc_t avc;
	uint8_t extra_data[64 * 1024];

    const uint8_t* frame = h264_start_code(ptr, end);
	const uint8_t* nalu = frame;
	while (nalu < end)
	{
        const unsigned char* nalu2 = h264_start_code(nalu + 4, end);
		int nal_unit_type = h264_nal_type(nalu);
        assert(0 != nal_unit_type);
        if(nal_unit_type <= 5)
        {
			// process one frame
			size_t bytes = nalu2 - frame;
			assert(bytes < sizeof(s_buffer));
			size_t n = mpeg4_annexbtomp4(&avc, frame, bytes, s_buffer, sizeof(s_buffer));
			frame = nalu2; // next frame

			if (!spspps)
			{
				if (!avc.chroma_format_idc || avc.nb_sps < 1 || avc.sps[0].bytes < 4)
					continue; // wait for key frame

				spspps = true;
				int extra_data_size = mpeg4_avc_decoder_configuration_record_save(&avc, extra_data, sizeof(extra_data));
				assert(extra_data_size > 0); // check buffer length
				track = mov_writer_add_video(mov, MOV_OBJECT_H264, width, height, extra_data, extra_data_size);
			}

			mov_writer_write(mov, track, s_buffer, n, pts, pts, avc.chroma_format_idc ? MOV_AV_FLAG_KEYFREAME : 0);
			pts += 40;
        }

        nalu = nalu2;
    }
}

void mov_writer_h264(const char* h264, int width, int height, const char* mp4)
{
	long bytes = 0;
	uint8_t* ptr = file_read(h264, &bytes);
	if (NULL == ptr) return;

	FILE* fp = fopen(mp4, "wb+");
	mov_writer_t* mov = mov_writer_create(mov_file_buffer(), fp, MOV_FLAG_FASTSTART);
	h264_read_frame(mov, width, height, ptr, ptr + bytes);
	mov_writer_destroy(mov);

	fclose(fp);
	free(ptr);
}
