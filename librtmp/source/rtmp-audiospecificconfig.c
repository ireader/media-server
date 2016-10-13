#include "rtmp-client.h"
#include <assert.h>
#include <stdio.h>
#include "mpeg4-aac.h"

size_t rtmp_client_make_AudioSpecificConfig(void* out, const void* audio, size_t bytes)
{
	int r;
	struct mpeg4_aac_t aac;

	r = mpeg4_aac_adts_load((const uint8_t*)audio, bytes, &aac);
	if (r < 0)
	{
		printf("audio don't have ADTS header\n");
		return 0;
	}

	return mpeg4_aac_audio_specific_config_save(&aac, out, bytes);
}
