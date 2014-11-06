#ifndef _mpeg_util_h_
#define _mpeg_util_h_

#include "mpeg-types.h"

// Intel/AMD little endian
// 0x01020304 -> |04|03|02|01|
void le_read_uint16(uint8_t* ptr, uint16_t* val);
void le_read_uint32(uint8_t* ptr, uint32_t* val);
void le_read_uint64(uint8_t* ptr, uint64_t* val);
void le_write_uint16(uint8_t* ptr, uint16_t val);
void le_write_uint32(uint8_t* ptr, uint32_t val);
void le_write_uint64(uint8_t* ptr, uint64_t val);

// ARM/Motorola big endian(network byte order)
// 0x01020304 -> |01|02|03|04|
void be_read_uint16(uint8_t* ptr, uint16_t* val);
void be_read_uint32(uint8_t* ptr, uint32_t* val);
void be_read_uint64(uint8_t* ptr, uint64_t* val);
void be_write_uint16(uint8_t* ptr, uint16_t val);
void be_write_uint32(uint8_t* ptr, uint32_t val);
void be_write_uint64(uint8_t* ptr, uint64_t val);

// The Internet Protocol defines big-endian as the standard network byte order
#define nbo_r16 be_read_uint16
#define nbo_r32 be_read_uint32
#define nbo_r64 be_read_uint64
#define nbo_w16 be_write_uint16
#define nbo_w32 be_write_uint32
#define nbo_w64 be_write_uint64

void pcr_write(uint8_t *ptr, int64_t pcr);

#endif /* !_mpeg_util_h_ */
