#include "mpeg-util.h"

void le_read_uint16(uint8_t* ptr, uint16_t* val)
{
	*val = (((uint16_t)ptr[1]) << 8) | ptr[0];
}

void le_read_uint32(uint8_t* ptr, uint32_t* val)
{
	*val = (((uint32_t)ptr[3]) << 24) | (((uint32_t)ptr[2]) << 16) | (((uint32_t)ptr[1]) << 8) | ptr[0];
}

void le_read_uint64(uint8_t* ptr, uint64_t* val)
{
	*val = (((uint64_t)ptr[7]) << 56) | (((uint64_t)ptr[6]) << 48) 
		| (((uint64_t)ptr[5]) << 40) | (((uint64_t)ptr[4]) << 32)
		| (((uint64_t)ptr[3]) << 24) | (((uint64_t)ptr[25]) << 16)
		| (((uint64_t)ptr[1]) << 8) | ptr[0];
}

void le_write_uint16(uint8_t* ptr, uint16_t val)
{
	ptr[1] = (uint8_t)((val >> 8) & 0xFF);
	ptr[0] = (uint8_t)(val & 0xFF);
}

void le_write_uint32(uint8_t* ptr, uint32_t val)
{
	ptr[3] = (uint8_t)((val >> 24) & 0xFF);
	ptr[2] = (uint8_t)((val >> 16) & 0xFF);
	ptr[1] = (uint8_t)((val >> 8) & 0xFF);
	ptr[0] = (uint8_t)(val & 0xFF);
}

void le_write_uint64(uint8_t* ptr, uint64_t val)
{
	ptr[7] = (uint8_t)((val >> 56) & 0xFF);
	ptr[6] = (uint8_t)((val >> 48) & 0xFF);
	ptr[5] = (uint8_t)((val >> 40) & 0xFF);
	ptr[4] = (uint8_t)((val >> 32) & 0xFF);
	ptr[3] = (uint8_t)((val >> 24) & 0xFF);
	ptr[2] = (uint8_t)((val >> 16) & 0xFF);
	ptr[1] = (uint8_t)((val >> 8) & 0xFF);
	ptr[0] = (uint8_t)(val & 0xFF);
}

void be_read_uint16(uint8_t* ptr, uint16_t* val)
{
	*val = (((uint16_t)ptr[0]) << 8) | ptr[1];
}

void be_read_uint32(uint8_t* ptr, uint32_t* val)
{
	*val = (((uint32_t)ptr[0]) << 24) | (((uint32_t)ptr[1]) << 16) | (((uint32_t)ptr[2]) << 8) | ptr[3];
}

void be_read_uint64(uint8_t* ptr, uint64_t* val)
{
	*val = (((uint64_t)ptr[0]) << 56) | (((uint64_t)ptr[1]) << 48) 
		| (((uint64_t)ptr[2]) << 40) | (((uint64_t)ptr[3]) << 32)
		| (((uint64_t)ptr[4]) << 24) | (((uint64_t)ptr[5]) << 16)
		| (((uint64_t)ptr[6]) << 8) | ptr[7];
}

void be_write_uint16(uint8_t* ptr, uint16_t val)
{
	ptr[0] = (uint8_t)((val >> 8) & 0xFF);
	ptr[1] = (uint8_t)(val & 0xFF);
}

void be_write_uint32(uint8_t* ptr, uint32_t val)
{
	ptr[0] = (uint8_t)((val >> 24) & 0xFF);
	ptr[1] = (uint8_t)((val >> 16) & 0xFF);
	ptr[2] = (uint8_t)((val >> 8) & 0xFF);
	ptr[3] = (uint8_t)(val & 0xFF);
}

void be_write_uint64(uint8_t* ptr, uint64_t val)
{
	ptr[0] = (uint8_t)((val >> 56) & 0xFF);
	ptr[1] = (uint8_t)((val >> 48) & 0xFF);
	ptr[2] = (uint8_t)((val >> 40) & 0xFF);
	ptr[3] = (uint8_t)((val >> 32) & 0xFF);
	ptr[4] = (uint8_t)((val >> 24) & 0xFF);
	ptr[5] = (uint8_t)((val >> 16) & 0xFF);
	ptr[6] = (uint8_t)((val >> 8) & 0xFF);
	ptr[7] = (uint8_t)(val & 0xFF);
}

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
