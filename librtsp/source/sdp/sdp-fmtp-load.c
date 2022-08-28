#include "sdp-a-fmtp.h"
#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "base64.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

/// @return >0-ok, other-error
int mpeg4_avc_from_fmtp(struct mpeg4_avc_t* avc, const struct sdp_a_fmtp_h264_t* fmtp)
{
	uint8_t t;
	int n, off;
	const char* p, * next;

	memset(avc, 0, sizeof(*avc));
	avc->nalu = 4;
	if (3 != sscanf(fmtp->profile_level_id, "%2hhx%2hhx%2hhx", &avc->profile, &avc->compatibility, &avc->level))
		return -EINVAL;

	off = 0;
	p = fmtp->sprop_parameter_sets;
	while (p)
	{
		next = strchr(p, ',');
		n = next ? (int)(next - p) : (int)strlen(p);
		if (off + (n + 3) / 4 * 3 > sizeof(avc->data))
			return -ENOMEM; // don't have enough space

		n = (int)base64_decode(avc->data + off, p, n);
		t = avc->data[off] & 0x1f;
		if (7 == t)
		{
			avc->sps[avc->nb_sps].data = avc->data + off;
			avc->sps[avc->nb_sps].bytes = (uint16_t)n;
			++avc->nb_sps;
			off += n;
		}
		else if (8 == t)
		{
			avc->pps[avc->nb_pps].data = avc->data + off;
			avc->pps[avc->nb_pps].bytes = (uint16_t)n;
			++avc->nb_pps;
			off += n;
		}
		else
		{
			assert(0);
		}

		p = next ? next + 1 : NULL;
	}

	return 0;
}

int mpeg4_hevc_from_fmtp(struct mpeg4_hevc_t* hevc, const struct sdp_a_fmtp_h265_t* fmtp)
{
	int i, n, off;
	const char* p, * next;
	const char* sprops[4];

	memset(hevc, 0, sizeof(*hevc));
	hevc->lengthSizeMinusOne = 3;
	hevc->array_completeness = 1;
	hevc->configurationVersion = 1;

	off = 0;
	sprops[0] = fmtp->sprop_vps;
	sprops[1] = fmtp->sprop_sps;
	sprops[2] = fmtp->sprop_pps;
	sprops[3] = fmtp->sprop_sei;
	for (i = 0; i < sizeof(sprops) / sizeof(sprops[0]); i++)
	{
		p = sprops[i];
		while (p)
		{
			next = strchr(p, ',');
			n = next ? (int)(next - p) : (int)strlen(p);
			if (off + (n + 3) / 4 * 3 > sizeof(hevc->data))
				return -ENOMEM; // don't have enough space

			n = (int)base64_decode(hevc->data + off, p, n);
			hevc->nalu[hevc->numOfArrays].data = hevc->data + off;
			hevc->nalu[hevc->numOfArrays].bytes = (uint16_t)n;
			hevc->nalu[hevc->numOfArrays].type = (hevc->data[off] > 1) & 0x3F;
			hevc->nalu[hevc->numOfArrays].array_completeness = 1;
			off += n;

			p = next ? next + 1 : NULL;
		}
	}

	return 0;
}

/// @return >0-ok, <=0-error
int aac_from_sdp_latm_config(struct mpeg4_aac_t* aac, struct sdp_a_fmtp_mpeg4_t* fmtp)
{
	int n;
	uint8_t buf[128];

	n = (int)strlen(fmtp->config);
	if (n / 2 > sizeof(buf))
		return -E2BIG;

	n = (int)base16_decode(buf, fmtp->config, n);
	return mpeg4_aac_stream_mux_config_load(buf, n, aac);
}

/// @return >0-ok, <=0-error
int aac_from_sdp_mpeg4_config(struct mpeg4_aac_t* aac, struct sdp_a_fmtp_mpeg4_t* fmtp)
{
	int n;
	uint8_t buf[128];

	n = (int)strlen(fmtp->config);
	if ((n + 3) / 4 * 3 > sizeof(buf))
		return -E2BIG;

	n = (int)base64_decode(buf, fmtp->config, n);
	return mpeg4_aac_audio_specific_config_load(buf, n, aac);
}
