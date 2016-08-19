#include "mpeg-util.h"

void pcr_write(uint8_t *ptr, int64_t pcr)
{
	int64_t pcr_base = pcr / 300;
	int64_t pcr_ext = pcr % 300;

	ptr[0] = (pcr_base >> 25) & 0xFF;
	ptr[1] = (pcr_base >> 17) & 0xFF;
	ptr[2] = (pcr_base >> 9) & 0xFF;
	ptr[3] = (pcr_base >> 1) & 0xFF;
	ptr[4] = ((pcr_base & 0x01) << 7) | 0x7E | ((pcr_ext>>8) & 0x01);
	ptr[5] = pcr_ext & 0xFF;
}
