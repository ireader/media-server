#include "rtmp-client.h"
#include <assert.h>
#include <stdio.h>

typedef unsigned char uint8_t;

int rtmp_client_make_AudioSpecificConfig(void* out, const void* audio, unsigned int bytes)
{
	uint8_t aacProfile, aacFrequence, aacChannel;
	const uint8_t* aac = (const uint8_t*)audio;
	uint8_t *p = (uint8_t*)out;

	if (bytes < 7)
	{
		printf("audio don't have ADTS header\n");
		return -1;
	}

	// AudioSpecificConfig
	// ISO-14496-3 Audio
	assert(0xFF == aac[0] && 0xF0 == (aac[1] & 0xF0));
	aacProfile = ((aac[2] >> 6) & 0x03) + 1; // 2 bits: the MPEG-4 Audio Object Type minus 1
	aacFrequence = (aac[2] >> 2) & 0x0F; // 4 bits: MPEG-4 Sampling Frequency Index (15 is forbidden)
	aacChannel = ((aac[2] & 0x01) << 2) | ((aac[3] >> 6) & 0x03); // 3 bits: MPEG-4 Channel Configuration 
	p[0] = (aacProfile << 3) | ((aacFrequence >> 1) & 0x07);
	p[1] = ((aacProfile & 0x01) << 7) | (aacChannel << 3) | 0x00;

	return 2;
}
