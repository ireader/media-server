#include "rtmp-handshake.h"
#include "rtmp-util.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <math.h>

static void rtmp_handshake_random(uint8_t* p, uint32_t timestamp)
{
	int i;
	
	srand(timestamp * (unsigned int)&timestamp * (unsigned int)p);
	for (i = 0; i * 4 < RTMP_HANDSHAKE - 8; i++)
	{
		*((int*)p + i) = rand();
	}
}

int rtmp_handshake_c0(uint8_t* c0, int version)
{
	assert(RTMP_VERSION == version);
	*c0 = (uint8_t)version;
	return 1;
}

int rtmp_handshake_c1(uint8_t* c1, uint32_t timestamp)
{
	be_write_uint32(c1, timestamp);
	be_write_uint32(c1 + 4, 0);
	rtmp_handshake_random(c1 + 8, timestamp);
	return RTMP_HANDSHAKE;
}

int rtmp_handshake_c2(uint8_t* c2, uint32_t timestamp, const uint8_t* s1, size_t bytes)
{
	assert(RTMP_HANDSHAKE == bytes);
	memcpy(c2, s1, bytes);
	be_write_uint32(c2 + 4, timestamp);
	return bytes;
}

int rtmp_handshake_s0(uint8_t* s0, int version)
{
	assert(RTMP_VERSION == version);
	*s0 = (uint8_t)version;
	return 1;
}

int rtmp_handshake_s1(uint8_t* s1, uint32_t timestamp)
{
	be_write_uint32(s1, timestamp);
	be_write_uint32(s1 + 4, 0);
	rtmp_handshake_random(s1 + 8, timestamp);
	return RTMP_HANDSHAKE;
}

int rtmp_handshake_s2(uint8_t* s2, uint32_t timestamp, const uint8_t* c1, size_t bytes)
{
	assert(RTMP_HANDSHAKE == bytes);
	memcpy(s2, c1, bytes);
	be_write_uint32(s2 + 4, timestamp);
	return bytes;
}
