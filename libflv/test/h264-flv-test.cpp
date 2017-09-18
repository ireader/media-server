#include "flv-writer.h"
#include "flv-muxer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static const uint8_t* search_start_code(const uint8_t* ptr, const uint8_t* end)
{
	for (const uint8_t *p = ptr; p + 3 < end; p++)
	{
		if (0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00 == p[2] && 0x01 == p[3])))
			return p;
	}
	return end;
}

static int on_flv_packet(void* flv, int type, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, type, data, bytes, timestamp);
}

void avc2flv_test(const char* inputH264, const char* outputFLV)
{
	void* f = flv_writer_create(outputFLV);
	flv_muxer_t* m = flv_muxer_create(on_flv_packet, f);
	FILE* fp = fopen(inputH264, "rb");

	uint32_t pts = 0, dts = 0;
	static uint8_t buffer[1 * 1024 * 1024];
	size_t n = fread(buffer, 1, sizeof(buffer), fp);
	const uint8_t* end = buffer + n;
	const uint8_t* nalu = search_start_code(buffer, end);
	while (nalu < end)
	{
		const uint8_t* next = search_start_code(nalu + 4, end);
		while (next < end)
		{
			nalu += 1 == nalu[2] ? 3 : 4;
			flv_muxer_h264_nalu(m, nalu, next - nalu, pts, dts);
			uint8_t type = *nalu & 0x1f;
			if (type > 0 && type < 6)
			{
				pts += 40;
				dts += 40;
			}

			nalu = next;
			next = search_start_code(nalu + 4, end);
		}
		
		n = end - nalu;
		if (0 == n)
			break; // too big H.264 NAL unit ( >4M )

		memmove(buffer, nalu, n);
		nalu = buffer;

		int v = fread(buffer + n, 1, sizeof(buffer) - n, fp);
		if (0 == v)
		{
			// EOF
			nalu += 1 == nalu[2] ? 3 : 4;
			flv_muxer_h264_nalu(m, nalu, end - nalu, 0, 0);
			break;
		}
		else
		{
			end = buffer + n + v;
		}
	}

	fclose(fp);
	flv_muxer_destroy(m);
	flv_writer_destroy(f);
}
