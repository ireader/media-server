#ifndef _AAC_ADTS_H_
#define _AAC_ADTS_H_

#include <assert.h>

struct aac_adts_t
{
	// adts fixed header
	unsigned char ID; // 0-mpeg4, 1-mpeg2
	unsigned char profile; // 0-main profile, 1-Low Complexity profile(LC), 2-Scalabe Sampling Rate profile(SSR), 3-reserved
	unsigned char sampling_frequency_index; // 0-96000, 1-88200, 2-64000, 3-48000, 4-44100, 5-32000, 6-24000, 7-22050, 8-16000, 9-12000, 10-11025, 11-8000, 12-7350, 13/14-reserved, 15-frequency is written explictly
	unsigned char channel_configuration; // 0-AOT, 1-1channel,front-center, 2-2channels, front-left/right, 3-3channels: front center/left/right, 4-4channels: front-center/left/right, back-center, 5-5channels: front center/left/right, back-left/right, 6-6channels: front center/left/right, back left/right LFE-channel, 7-8channels

	// adts variable header
	unsigned short aac_frame_length; // include adts and es stream
};

// ISO-14496-3 adts_frame
static int aac_adts_load(const unsigned char* data, unsigned int bytes, struct aac_adts_t* adts)
{
	if (bytes < 4) return -1;

	assert(0xFF == data[0] && 0xF0 == (data[1] & 0xF0)); /* syncword */
	adts->ID = (data[1] >> 3) & 0x01;
	adts->profile = ((data[2] >> 6) & 0x03); // 2 bits: the MPEG-4 Audio Object Type minus 1
	adts->sampling_frequency_index = (data[2] >> 2) & 0x0F; // 4 bits: MPEG-4 Sampling Frequency Index (15 is forbidden)
	adts->channel_configuration = ((data[2] & 0x01) << 2) | ((data[3] >> 6) & 0x03); // 3 bits: MPEG-4 Channel Configuration 
	return 0;
}

// ISO-14496-3 adts_frame (p122)
/*
adts_fixed_header()
{
	syncword;					12 bslbf
	ID;							1 bslbf
	layer;						2 uimsbf
	protection_absent;			1 bslbf
	profile_ObjectType;			2 uimsbf
	sampling_frequency_index;	4 uimsbf
	private_bit;				1 bslbf
	channel_configuration;		3 uimsbf
	original_copy;				1 bslbf
	home;						1 bslbf
}

adts_variable_header()
{
	copyright_identification_bit;		1 bslbf
	copyright_identification_start;		1 bslbf
	aac_frame_length;					13 bslbf
	adts_buffer_fullness;				11 bslbf
	number_of_raw_data_blocks_in_frame; 2 uimsbf
}
*/
static int aac_adts_save(const struct aac_adts_t* adts, unsigned char* data, uint32_t bytes)
{
	uint16_t len = adts->aac_frame_length + 7;
	if (bytes < 7) return -1;

	data[0] = 0xFF; /* 12-syncword */
	data[1] = 0xF0 /* 12-syncword */ | ((adts->ID & 0x01) << 3) | 0x00 /*2-layer*/ | 0x01 /*1-protection_absent*/;
	data[2] = (adts->profile << 6) | ((adts->sampling_frequency_index & 0x0F) << 2) | ((adts->channel_configuration >> 2) & 0x01);
	data[3] = (adts->channel_configuration << 6) | ((len >> 11) & 0x03); /*0-original_copy*/ /*0-home*/ /*0-copyright_identification_bit*/ /*0-copyright_identification_start*/
	data[4] = (unsigned char)(len >> 3);
	data[5] = ((len & 0x07) << 5) | 0x1F;
	data[6] = 0xFC | ((adts->aac_frame_length/1024) & 0x03);
	return 0;
}

// ISO-14496-3 AudioSpecificConfig (p52)
/*
audioObjectType;								5 uimsbf
if (audioObjectType == 31) {
	audioObjectType = 32 + audioObjectTypeExt;	6 uimsbf
}
samplingFrequencyIndex;							4 bslbf
if ( samplingFrequencyIndex == 0xf ) {
	samplingFrequency;							24 uimsbf
}
channelConfiguration;							4 bslbf
*/
static int aac_adts_from_AudioSpecificConfig(const unsigned char* data, unsigned int bytes, struct aac_adts_t* adts)
{
	if (bytes < 2) return -1;

	adts->profile = ((data[0] >> 3) & 0x1F) - 1;
	adts->sampling_frequency_index = ((data[0] & 0x7) << 1) | ((data[1] >> 7) & 0x01);
	adts->channel_configuration = (data[1] >> 3) & 0x0F;
	return 0;
}

// ISO-14496-3 AudioSpecificConfig
static int aac_adts_to_AudioSpecificConfig(const struct aac_adts_t* adts, unsigned char* data, unsigned int bytes)
{
	if (bytes < 2) return -1;

	data[0] = ((adts->profile+1) << 3) | ((adts->sampling_frequency_index >> 1) & 0x07);
	data[1] = ((adts->sampling_frequency_index & 0x01) << 7) | (adts->channel_configuration << 3) | 0x00;
	return 0;
}

#endif /* !_AAC_ADTS_H_ */
