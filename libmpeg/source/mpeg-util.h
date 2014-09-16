#ifndef _mpeg_util_h_
#define _mpeg_util_h_

#include "mpeg-types.h"

// ARM/Motorola little endian(network byte order)
// 0x01020304 -> |01|02|03|04|
void le_read_uint16(uint8_t* ptr, uint16_t* val);
void le_read_uint32(uint8_t* ptr, uint32_t* val);
void le_read_uint64(uint8_t* ptr, uint64_t* val);
void le_write_uint16(uint8_t* ptr, uint16_t val);
void le_write_uint32(uint8_t* ptr, uint32_t val);
void le_write_uint64(uint8_t* ptr, uint64_t val);

// Intel/AMD big endian
// 0x01020304 -> |04|03|02|01|
void be_read_uint16(uint8_t* ptr, uint16_t* val);
void be_read_uint32(uint8_t* ptr, uint32_t* val);
void be_read_uint64(uint8_t* ptr, uint64_t* val);
void be_write_uint16(uint8_t* ptr, uint16_t val);
void be_write_uint32(uint8_t* ptr, uint32_t val);
void be_write_uint64(uint8_t* ptr, uint64_t val);

void pcr_write(uint8_t *ptr, int64_t pcr);

#endif /* !_mpeg_util_h_ */
