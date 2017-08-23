#ifndef _mpeg4_aac_h_
#define _mpeg4_aac_h_

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct mpeg4_aac_t
{
	uint8_t profile; // 0-NULL, 1-AAC Main, 2-AAC LC, 2-AAC SSR, 3-AAC LTP

	uint8_t sampling_frequency_index; // 0-96000, 1-88200, 2-64000, 3-48000, 4-44100, 5-32000, 6-24000, 7-22050, 8-16000, 9-12000, 10-11025, 11-8000, 12-7350, 13/14-reserved, 15-frequency is written explictly

	uint8_t channel_configuration; // 0-AOT, 1-1channel,front-center, 2-2channels, front-left/right, 3-3channels: front center/left/right, 4-4channels: front-center/left/right, back-center, 5-5channels: front center/left/right, back-left/right, 6-6channels: front center/left/right, back left/right LFE-channel, 7-8channels
};

enum mpeg2_aac_profile
{
	MPEG2_AAC_MAIN = 0,
	MPEG2_AAC_LC,
	MPEG2_AAC_SSR,
};

enum mpeg4_aac_profile
{
	MPEG4_AAC_MAIN = 1,
	MPEG4_AAC_LC,
	MPEG4_AAC_SSR,
	MPEG4_AAC_LTP,
	MPEG4_AAC_SBR,
	MPEG4_AAC_SCALABLE,
};

enum mpeg4_aac_frequency
{
	MPEG4_AAC_96000 = 0,
	MPEG4_AAC_88200,	// 0x1
	MPEG4_AAC_64000,	// 0x2
	MPEG4_AAC_48000,	// 0x3
	MPEG4_AAC_44100,	// 0x4
	MPEG4_AAC_32000,	// 0x5
	MPEG4_AAC_24000,	// 0x6
	MPEG4_AAC_22050,	// 0x7
	MPEG4_AAC_16000,	// 0x8
	MPEG4_AAC_12000,	// 0x9
	MPEG4_AAC_11025,	// 0xa
	MPEG4_AAC_8000,		// 0xb
	MPEG4_AAC_7350,		// 0xc
						// reserved
						// reserved
						// escape value

};

/// @return >=0-adts header length, <0-error
int mpeg4_aac_adts_save(const struct mpeg4_aac_t* aac, size_t payload, uint8_t* data, size_t bytes);
/// @return >=0-adts header length, <0-error
int mpeg4_aac_adts_load(const uint8_t* data, size_t bytes, struct mpeg4_aac_t* aac);

/// @return >=0-audio specific config length, <0-error
int mpeg4_aac_audio_specific_config_load(const uint8_t* data, size_t bytes, struct mpeg4_aac_t* aac);
/// @return >=0-audio specific config length, <0-error
int mpeg4_aac_audio_specific_config_save(const struct mpeg4_aac_t* aac, uint8_t* data, size_t bytes);

/// @return >=0-stream mux config length, <0-error
int mpeg4_aac_stream_mux_config_save(const struct mpeg4_aac_t* aac, uint8_t* data, size_t bytes);

/// get AAC profile level indication value
int mpeg4_aac_profile_level(const struct mpeg4_aac_t* aac);

/// MPEG4_AAC_96000 => 96000
/// @return -1-error, other-frequency value
int mpeg4_aac_audio_frequency_to(enum mpeg4_aac_frequency index);
/// 96000 => MPEG4_AAC_96000
/// @return -1-error, other-frequency index
int mpeg4_aac_audio_frequency_from(int frequency);

#if defined(__cplusplus)
}
#endif
#endif /* !_mpeg4_aac_h_ */
