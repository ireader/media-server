#ifndef _rtmp_handshake_h_
#define _rtmp_handshake_h_

#include <stdint.h>
#include <stddef.h>

enum
{
	RTMP_VERSION	= 3,
	RTMP_HANDSHAKE	= 1536,
};

int rtmp_handshake_c0(uint8_t* c0, int version);
int rtmp_handshake_c1(uint8_t* c1, uint32_t timestamp);
int rtmp_handshake_c2(uint8_t* c2, uint32_t timestamp, const uint8_t* s1, size_t bytes);

int rtmp_handshake_s0(uint8_t* s0, int version);
int rtmp_handshake_s1(uint8_t* s1, uint32_t timestamp);
int rtmp_handshake_s2(uint8_t* s2, uint32_t timestamp, const uint8_t* c1, size_t bytes);

#endif /* !_rtmp_handshake_h_ */
