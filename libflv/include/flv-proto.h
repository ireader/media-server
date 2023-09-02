#ifndef _flv_proto_h_
#define _flv_proto_h_

// FLV Tag Type
#define FLV_TYPE_AUDIO		8
#define FLV_TYPE_VIDEO		9
#define FLV_TYPE_SCRIPT		18

// FLV Audio Type
#define FLV_AUDIO_LPCM		(0 << 4) // Linear PCM, platform endian
#define FLV_AUDIO_ADPCM		(1 << 4)
#define FLV_AUDIO_MP3		(2 << 4)
#define FLV_AUDIO_LLPCM		(3 << 4) // Linear PCM, little endian
#define FLV_AUDIO_G711A		(7 << 4) // G711 A-law
#define FLV_AUDIO_G711U     (8 << 4) // G711 mu-law
#define FLV_AUDIO_AAC		(10 << 4)
#define FLV_AUDIO_SPEEX		(11 << 4)
#define FLV_AUDIO_OPUS		(13 << 4)
#define FLV_AUDIO_MP3_8K	(14 << 4) // MP3 8 kHz
#define FLV_AUDIO_DEVIDE	(15 << 4) // Device-specific sound
#define FLV_AUDIO_ASC		(0x1000 | FLV_AUDIO_AAC)	// AudioSpecificConfig(ISO-14496-3)
#define FLV_AUDIO_OPUS_HEAD	(0x1100 | FLV_AUDIO_OPUS)// opus-codec.org

// FLV Video Type
#define FLV_VIDEO_H263		2 // Sorenson H.263
#define FLV_VIDEO_SCREEN	3 // Screen video
#define FLV_VIDEO_VP6		4 // On2 VP6
#define FLV_VIDEO_H264		7 // AVC
#define FLV_VIDEO_H265		12 // https://github.com/CDN-Union/H265
#define FLV_VIDEO_AV1		13 // https://aomediacodec.github.io/av1-isobmff
#define FLV_VIDEO_AVS3		14
#define FLV_VIDEO_H266		15
#define FLV_VIDEO_AVCC		(0x2000 | FLV_VIDEO_H264) // AVCDecoderConfigurationRecord(ISO-14496-15)
#define FLV_VIDEO_HVCC		(0x2100 | FLV_VIDEO_H265) // HEVCDecoderConfigurationRecord(ISO-14496-15)
#define FLV_VIDEO_AV1C		(0x2200 | FLV_VIDEO_AV1)  // AV1CodecConfigurationRecord(av1-isobmff)
#define FLV_VIDEO_AVSC		(0x2300 | FLV_VIDEO_AVS3) // AVS3DecoderConfigurationRecord
#define FLV_VIDEO_VVCC		(0x2400 | FLV_VIDEO_H266) // VVCDecoderConfigurationRecord(ISO-14496-15)

#define FLV_SCRIPT_METADATA	0x4000 // onMetaData

enum
{
	FLV_SEQUENCE_HEADER					= 0, // AVC/AAC sequence header
	FLV_AVPACKET						= 1, // AVC NALU / AAC raw
	FLV_END_OF_SEQUENCE					= 2, // AVC end of sequence (lower level NALU sequence ender is not required or supported)

	// CompositionTime Offset is implied to equal zero. This is
	// an optimization to save putting SI24 composition time value of zero on
	// the wire. See pseudo code below in the VideoTagBody section
	FLV_PACKET_TYPE_CODED_FRAMES_X = 3,

	// VideoTagBody does not contain video data. VideoTagBody
	// instead contains an AMF encoded metadata. See Metadata Frame
	// section for an illustration of its usage. As an example, the metadata
	// can be HDR information. This is a good way to signal HDR
	// information. This also opens up future ways to express additional
	// metadata that is meant for the next video sequence.
	//
	// note: presence of PacketTypeMetadata means that FrameType
	// flags at the top of this table should be ignored
	FLV_PACKET_TYPE_METADATA = 4,

	// Carriage of bitstream in MPEG-2 TS format
	// note: PacketTypeSequenceStart and PacketTypeMPEG2TSSequenceStart
	// are mutually exclusive
	FLV_PACKET_TYPE_MPEG2TS_SEQUENCE_START = 5,
};

enum
{
	FLV_VIDEO_KEY_FRAME					= 1, // key frame (for AVC, a seekable frame)
	FLV_VIDEO_INTER_FRAME				= 2, // inter frame (for AVC, a non-seekable frame)
	FLV_VIDEO_DISPOSABLE_INTER_FRAME	= 3, // H.263 only
	FLV_VIDEO_GENERATED_KEY_FRAME		= 4, // generated key frame (reserved for server use only)
	FLV_VIDEO_COMMAND_FRAME				= 5, // video info/command frame
};

enum
{
	FLV_SOUND_RATE_5500					= 0, // 5.5 kHz
	FLV_SOUND_RATE_11025				= 1, // 11 kHz
	FLV_SOUND_RATE_22050				= 2, // 22 kHz
	FLV_SOUND_RATE_44100				= 3, // 44 kHz
};

enum
{
	FLV_SOUND_BIT_8						= 0, // 8-bit samples
	FLV_SOUND_BIT_16					= 1, // 16-bit samples
};

enum
{
	FLV_SOUND_CHANNEL_MONO				= 0, // 1-channel
	FLV_SOUND_CHANNEL_STEREO			= 1, // 2-channels
};

#define FLV_VIDEO_FOURCC(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

#define FLV_VIDEO_FOURCC_AV1    FLV_VIDEO_FOURCC('a', 'v', '0', '1')
#define FLV_VIDEO_FOURCC_VP9    FLV_VIDEO_FOURCC('v', '9', '0', '9')
#define FLV_VIDEO_FOURCC_HEVC	FLV_VIDEO_FOURCC('h', 'v', 'c', '1')
#define FLV_VIDEO_FOURCC_VVC	FLV_VIDEO_FOURCC('v', 'v', 'c', '1')

#endif /* !_flv_proto_h_ */
