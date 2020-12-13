#include "webm-reader.h"
#include "webm-internal.h"

struct webm_reader_t
{
	struct webm_t webm;

	struct webm_buffer_t io;
	void* param;
	int error;
};

struct webm_parse_t
{
	uint32_t id;
	enum ebml_element_type_e type;
	int level;
	uint32_t parent;
	int(*parse)(struct webm_t* webm, const struct webm_box_t* box);
};

int mkv_parser(webm_reader_t* reader)
{

}

// https://github.com/cellar-wg/ebml-specification/blob/master/specification.markdown
enum ebml_element_type_e
{
	ebml_type_unknown,
	ebml_type_int, // Signed Integer Element [0-8]
	ebml_type_uint, // Unsigned Integer Element [0-8]
	ebml_type_float, // Float Element (0/4/8)
	ebml_type_string, // String Element [0-VINTMAX]
	ebml_type_utf8, // UTF-8 Element [0-VINTMAX]
	ebml_type_date, // Date Element [0-8]
	ebml_type_master, // Master Element [0-VINTMAX]
	ebml_type_binary, // Binary Element [0-VINTMAX]
};

enum {
	EBML_TRACK_VIDEO	= 1,
	EBML_TRACK_AUDIO	= 2,
	EBML_TRACK_COMPLEX	= 3,
	EBML_TRACK_LOGO		= 16,
	EBML_TRACK_SUBTITLE = 17,
	EBML_TRACK_BUTTONS	= 18,
	EBML_TRACK_CONTROL	= 32,
	EBML_TRACK_METADATA = 33,
};

enum ebml_video_interlaced_e 
{
	EBML_VIDEO_FLAG_UNDETERMINED = 0,
	EBML_VIDEO_FLAG_INTERLACED = 1,
	EBML_VIDEO_FLAG_PROGRESSIVE = 2,
};

enum ebml_video_field_order_e
{
	EBML_VIDEO_FIELD_ORDER_PROGRESSIVE = 0,
	EBML_VIDEO_FIELD_ORDER_TTF = 1,
	EBML_VIDEO_FIELD_ORDER_UNDETERMINED = 2,
	EBML_VIDEO_FIELD_ORDER_BFF = 6,
	EBML_VIDEO_FIELD_ORDER_BFF_SWAPPED = 9,
	EBML_VIDEO_FIELD_ORDER_TTF_SWAPPED = 14,
};

enum ebml_video_stereo_mode_e
{
	EBML_VIDEO_STEREO_MODE_MONO = 0,
	EBML_VIDEO_STEREO_MODE_SIDE_BY_SIDE_LEFT = 1, // left eye first
	EBML_VIDEO_STEREO_MODE_TOP_BOTTOM_RIGHT = 2, // right eye first
	EBML_VIDEO_STEREO_MODE_TOP_BOTTOM_LEFT = 3, // left eye first
	EBML_VIDEO_STEREO_MODE_CHECKBOARD_RIGHT = 4, // right eye first
	EBML_VIDEO_STEREO_MODE_CHECKBOARD_LEFT = 5, // left eye first
	EBML_VIDEO_STEREO_MODE_ROW_INTERLEAVED_RIGHT = 6, // right eye first
	EBML_VIDEO_STEREO_MODE_ROW_INTERLEAVED_LEFT = 7, // left eye first
	EBML_VIDEO_STEREO_MODE_COLUMN_INTERLEAVED_RIGHT = 8, // right eye first
	EBML_VIDEO_STEREO_MODE_COLUMN_INTERLEAVED_LEFT = 9, // left eye first
	EBML_VIDEO_STEREO_MODE_ANAGLYPH_CYAN_RED = 10, // cyan/red
	EBML_VIDEO_STEREO_MODE_SIDE_BY_SIDE_RIGHT = 11, // right eye first
	EBML_VIDEO_STEREO_MODE_ANAGLYPH_GREEN_MAGENTA = 12, // green/megenta
	EBML_VIDEO_STEREO_MODE_BOTH_EYES_LACED_IN_ONE_BLOCK_LEFT = 13, // left eye first
	EBML_VIDEO_STEREO_MODE_BOTH_EYES_LACED_IN_ONE_BLOCK_RIGHT = 14, // right eye first
};

enum ebml_video_display_unit_e
{
	EBML_VIDEO_DISPLAY_UNIT_PIXELS = 0,
	EBML_VIDEO_DISPLAY_UNIT_CETIMETERS = 1,
	EBML_VIDEO_DISPLAY_UNIT_INCHES = 2, 
	EBML_VIDEO_DISPLAY_UNIT_ASPECT_RATION = 3, 
	EBML_VIDEO_DISPLAY_UNIT_UNKNOWN = 4,
};

enum ebml_video_aspect_ratio_type_e
{
	EBML_VIDEO_ASPECT_RATIO_TYPE_FREE_RESIZING = 0,
	EBML_VIDEO_ASPECT_RATIO_TYPE_KEEP_ASPECT_RATIOn = 1,
	EBML_VIDEO_ASPECT_RATIO_TYPE_FIXED = 2,
};

enum ebml_video_color_matrix_coefficients_t
{
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_IDENTIY = 0, 
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_ITU_R_BT709 = 1,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_UNSPECIFIED = 2,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_RESERVED = 3,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_US_FCC_73_682 = 4,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_ITU_R_BT_470BG = 5,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_SMPTE_170M = 6,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_SMPTE_240M = 7,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_YCOCG = 8,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_BT2020_NON_CONSTANT_LUMINANCE = 9,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_BT2020_CONSTANT_LUMINANCE = 10,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_SMPTE_ST_2085 = 11,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NON_CONSTANT_LUMINANCE = 12,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CONSTANT_LUMINANCE = 13,
	EBML_VIDEO_COLOR_MATRIX_COEFFICIENTS_ITU_R_BT_2100_0 = 14,
};

enum ebml_video_color_range_e
{
	EBML_VIDEO_COLOR_RANGE_UNSPECIFIED = 0,
	EBML_VIDEO_COLOR_RNAGE_BROADCAST_RANGE,
	EBML_VIDEO_COLOR_RNAGE_FULL_RANGE,
	EBML_VIDEO_COLOR_RNAGE_MATRIX_COEFFICIENTS,
};

enum ebml_video_color_transfer_characteristics_e
{
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_RESERVED = 0,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_ITU_R_BT_709 = 1,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_UNSPECIFIED = 2,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_GAMMA_2_2_CURVE_BT_470M = 4,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_GAMMA_2_8_CURVE_BT_470BG = 5,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_SMPTE_170M = 6,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_SMPTE_240M = 7,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_LINEAR = 8,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_LOG = 9,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_LOG_SQRT = 10,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_IEC_61966_2_4 = 11,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_ITU_R_BT_1361_EXTENDED_COLOUR_GAMUT = 12,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_IEC_61966_2_1 = 13,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_ITU_R_BT_2020_10BIT = 14,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_ITU_R_BT_2020_12BIT = 15,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_ITU_R_BT_2100_PERCEPTUAL_QUANTIZATION = 16,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_SMPTE_ST_428_1 = 17,
	EBML_VIDEO_COLOR_TRANSFER_CHARACTERISTICS_ARIB_STD_B67_HLG = 18,
};

enum ebml_video_color_primaries_e
{
	EBML_VIDEO_COLOR_PRIMARIES_RESERVED = 0,
	EBML_VIDEO_COLOR_PRIMARIES_ITU_R_BT_709 = 1,
	EBML_VIDEO_COLOR_PRIMARIES_UNSPECIFIED = 2, 
	EBML_VIDEO_COLOR_PRIMARIES_ITU_R_BT_470M = 4,
	EBML_VIDEO_COLOR_PRIMARIES_ITU_R_BT_470BG_BT_601_625 = 5,
	EBML_VIDEO_COLOR_PRIMARIES_ITU_R_BT_601_525_TMPTE_170M = 6,
	EBML_VIDEO_COLOR_PRIMARIES_SMPTE_240M = 7,
	EBML_VIDEO_COLOR_PRIMARIES_FILM = 8,
	EBML_VIDEO_COLOR_PRIMARIES_ITU_R_BT_2020 = 9,
	EBML_VIDEO_COLOR_PRIMARIES_SMPTE_ST_428_1 = 10,
	EBML_VIDEO_COLOR_PRIMARIES_SMPTE_RP_432_2 = 11,
	EBML_VIDEO_COLOR_PRIMARIES_SMPTE_EG_432_2 = 12,
	EBML_VIDEO_COLOR_PRIMARIES_EBU_TECH_3212E_JEDEC_P22_PHOSPHORS = 22,
};

enum ebml_video_projection_type_e
{
	EBML_VIDEO_PROJECTION_RECTANGULAR = 0,
	EBML_VIDEO_PROJECTION_EQUIRECTANGULAR = 1,
	EBML_VIDEO_PROJECTION_CUBEMAP = 2,
	EBML_VIDEO_PROJECTION_MESH = 3,
};

static int ebml_element_read(enum ebml_element_type_e type)
{
	switch (type)
	{
	case ebml_type_int:
		break;
	case ebml_type_uint:
		break;
	case ebml_type_float:
		break;
	case ebml_type_string:
		break;
	case ebml_type_utf8:
		break;
	case ebml_type_date:
		break;
	case ebml_type_master:
		break;
	case ebml_type_binary:
		break;
	default:
	}
}

static int ebml_doc_type_extersion_parse(webm_reader_t* reader)
{
}

static struct webm_parse_t s_elements[] = {
	// Global
	{ 0xBF,			ebml_type_binary,	-1,	0,			}, // CRC-32 Element, length 4
	{ 0xEC,			ebml_type_binary,	-1,	0,			}, // Void Element

	// EBML
	{ 0x1A45DFA3,	ebml_type_master,	0,	0, }, // EBML
	{ 0x4286,		ebml_type_uint,		1,	0x1A45DFA3,	}, // EBMLVersion, default 1
	{ 0x42F7,		ebml_type_uint,		1,	0x1A45DFA3,	}, // EBMLReadVersion, default 1
	{ 0x42F2,		ebml_type_uint,		1,	0x1A45DFA3,	}, // EBMLMaxIDLength, default 4
	{ 0x42F3,		ebml_type_uint,		1,	0x1A45DFA3,	}, // EBMLMaxSizeLength, default 8
	{ 0x4282,		ebml_type_string,	1,	0x1A45DFA3,	}, // DocType
	{ 0x4287,		ebml_type_uint,		1,	0x1A45DFA3,	}, // DocTypeVersion, default 1
	{ 0x4285,		ebml_type_uint,		1,	0x1A45DFA3,	}, // DocTypeReadVersion, default 1
	{ 0x4281,		ebml_type_master,	1,	0x1A45DFA3,	ebml_doc_type_extersion_parse }, // DocTypeExtension
	{ 0x4283,		ebml_type_string,	2,	0x4281,		}, // DocTypeExtensionName
	{ 0x4284,		ebml_type_uint,		2,	0x4281,		}, // DocTypeExtensionVersion

	// MKV
	{ 0x18538067,	ebml_type_master,	0,	0, }, // Segment

	// Meta Seek Information
	{ 0x114D9B74,	ebml_type_master,	1,	0x18538067, }, // Segment\SeekHead [mult]
	{ 0x4DBB,		ebml_type_master,	2,	0x114D9B74, }, // Segment\SeekHead\Seek [mult]
	{ 0x53AB,		ebml_type_binary,	3,	0x4DBB,		}, // Segment\SeekHead\Seek\SeekID
	{ 0x53AC,		ebml_type_uint,		4,	0x4DBB,		}, // Segment\SeekHead\Seek\SeekPosition

	// Segment Information
	{ 0x1549A966,	ebml_type_master,	1,	0x18538067, }, // Segment\Info [mult]
	{ 0x73A4,		ebml_type_binary,	2,	0x1549A966, }, // Segment\Info\SegmentUID
	{ 0x7384,		ebml_type_utf8,		2,	0x1549A966, }, // Segment\Info\SegmentFilename
	{ 0x3CB923,		ebml_type_binary,	2,	0x1549A966, }, // Segment\Info\PrevUID
	{ 0x3C83AB,		ebml_type_utf8,		2,	0x1549A966, }, // Segment\Info\PrevFilename
	{ 0x3EB923,		ebml_type_binary,	2,	0x1549A966, }, // Segment\Info\NextUID
	{ 0x3E83BB,		ebml_type_utf8,		2,	0x1549A966, }, // Segment\Info\NextFilename
	{ 0x4444,		ebml_type_binary,	2,	0x1549A966, }, // Segment\Info\SegmentFamily [mult]
	{ 0x6924,		ebml_type_master,	2,	0x1549A966, }, // Segment\Info\ChapterTranslate [mult]
	{ 0x69FC,		ebml_type_uint,		3,	0x6924,		}, // Segment\Info\ChapterTranslate\ChapterTranslateEditionUID [mult]
	{ 0x69BF,		ebml_type_uint,		3,	0x6924,		}, // Segment\Info\ChapterTranslate\ChapterTranslateCodec
	{ 0x69A5,		ebml_type_binary,	3,	0x6924,		}, // Segment\Info\ChapterTranslate\ChapterTranslateID
	{ 0x2AD7B1,		ebml_type_uint,		2,	0x1549A966,	}, // Segment\Info\TimestampScale, default 1000000
	{ 0x4489,		ebml_type_float,	2,	0x1549A966,	}, // Segment\Info\Duration
	{ 0x4461,		ebml_type_date,		2,	0x1549A966,	}, // Segment\Info\DateUTC
	{ 0x7BA9,		ebml_type_utf8,		2,	0x1549A966,	}, // Segment\Info\Title
	{ 0x4D80,		ebml_type_utf8,		2,	0x1549A966,	}, // Segment\Info\MuxingApp
	{ 0x5741,		ebml_type_utf8,		2,	0x1549A966,	}, // Segment\Info\WritingApp

	// Cluster
	{ 0x1F43B675,	ebml_type_master,	1,	0x18538067, }, // Segment\Cluster [mult]
	{ 0xE7,			ebml_type_uint,		2,	0x1F43B675, }, // Segment\Cluster\Timestamp
	{ 0x5854,		ebml_type_master,	2,	0x1F43B675, }, // Segment\Cluster\SilentTracks
	{ 0x58D7,		ebml_type_uint,		3,	0x5854,		}, // Segment\Cluster\SilentTracks\SilentTrackNumber [mult]
	{ 0xA7,			ebml_type_uint,		2,	0x1F43B675,	}, // Segment\Cluster\Position
	{ 0xAB,			ebml_type_uint,		2,	0x1F43B675,	}, // Segment\Cluster\PrevSize
	{ 0xA3,			ebml_type_binary,	2,	0x1F43B675,	}, // Segment\Cluster\SimpleBlock [mult]
	{ 0xA0,			ebml_type_master,	2,	0x1F43B675,	}, // Segment\Cluster\BlockGroup [mult]
	{ 0xA1,			ebml_type_binary,	3,	0xA0,		}, // Segment\Cluster\BlockGroup\Block
	{ 0xA2,			ebml_type_binary,	3,	0xA0,		}, // Segment\Cluster\BlockGroup\BlockVirtual
	{ 0x75A1,		ebml_type_master,	3,	0xA0,		}, // Segment\Cluster\BlockGroup\BlockAdditions
	{ 0xA6,			ebml_type_master,	4,	0x75A1,		}, // Segment\Cluster\BlockGroup\BlockAdditions\BlockMore [mult]
	{ 0xEE,			ebml_type_uint,		5,	0xA6,		}, // Segment\Cluster\BlockGroup\BlockAdditions\BlockMore\BlockAddID
	{ 0xA5,			ebml_type_binary,	5,	0xA6,		}, // Segment\Cluster\BlockGroup\BlockAdditions\BlockMore\BlockAdditional
	{ 0x9B,			ebml_type_uint,		3,	0xA0,		}, // Segment\Cluster\BlockGroup\BlockDuration
	{ 0xFA,			ebml_type_uint,		3,	0xA0,		}, // Segment\Cluster\BlockGroup\ReferencePriority, default 0
	{ 0xFB,			ebml_type_int,		3,	0xA0,		}, // Segment\Cluster\BlockGroup\ReferenceBlock [mult]
	{ 0xFD,			ebml_type_int,		3,	0xA0,		}, // Segment\Cluster\BlockGroup\ReferenceVirtual
	{ 0xA4,			ebml_type_binary,	3,	0xA0,		}, // Segment\Cluster\BlockGroup\CodecState
	{ 0x75A2,		ebml_type_int,		3,	0xA0,		}, // Segment\Cluster\BlockGroup\DiscardPadding
	{ 0x8E,			ebml_type_master,	3,	0xA0,		}, // Segment\Cluster\BlockGroup\Slices
	{ 0xE8,			ebml_type_master,	4,	0x8E,		}, // Segment\Cluster\BlockGroup\Slices\TimeSlice [mult]
	{ 0xCC,			ebml_type_uint,		5,	0xE8,		}, // Segment\Cluster\BlockGroup\Slices\TimeSlice\LaceNumber, default 0
	{ 0xCD,			ebml_type_uint,		5,	0xE8,		}, // Segment\Cluster\BlockGroup\Slices\TimeSlice\FrameNumber, default 0
	{ 0xCB,			ebml_type_uint,		5,	0xE8,		}, // Segment\Cluster\BlockGroup\Slices\TimeSlice\BlockAdditionID, default 0
	{ 0xCE,			ebml_type_uint,		5,	0xE8,		}, // Segment\Cluster\BlockGroup\Slices\TimeSlice\Delay, default 0
	{ 0xCF,			ebml_type_uint,		5,	0xE8,		}, // Segment\Cluster\BlockGroup\Slices\TimeSlice\SliceDuration, default 0
	{ 0xC8,			ebml_type_master,	3,	0xA0,		}, // Segment\Cluster\BlockGroup\ReferenceFrame
	{ 0xC9,			ebml_type_uint,		4,	0xC8,		}, // Segment\Cluster\BlockGroup\ReferenceFrame\ReferenceOffset
	{ 0xCA,			ebml_type_uint,		4,	0xC8,		}, // Segment\Cluster\BlockGroup\ReferenceFrame\ReferenceTimestamp
	{ 0xAF,			ebml_type_binary,	2,	0x1F43B675,	}, // Segment\Cluster\EncryptedBlock [mult]

	// Track
	{ 0x1654AE6B,	ebml_type_master,	1,	0x18538067, }, // Segment\Tracks
	{ 0xAE,			ebml_type_master,	2,	0x1654AE6B, }, // Segment\Tracks\TrackEntry [mult]
	{ 0xD7,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrackNumber
	{ 0x73C5,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrackUID
	{ 0x83,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrackType, [1-254]
	{ 0xB9,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\FlagEnabled, default 1
	{ 0x88,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\FlagDefault, default 1
	{ 0x55AA,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\FlagForced, default 0
	{ 0x9C,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\FlagLacing, default 1
	{ 0x6DE7,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\MinCache, default 0
	{ 0x6DF8,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\MaxCache
	{ 0x23E383,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\DefaultDuration
	{ 0x234E7A,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\DefaultDecodedFieldDuration
	{ 0x23314F,		ebml_type_float,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrackTimestampScale, default 1.0
	{ 0x537F,		ebml_type_int,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrackOffset, default 0
	{ 0x55EE,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\MaxBlockAdditionID, default 0
	{ 0x41E4,		ebml_type_master,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\BlockAdditionMapping [mult]
	{ 0x41F0,		ebml_type_uint,		4,	0x41E4,		}, // Segment\Tracks\TrackEntry\BlockAdditionMapping\BlockAddIDValue
	{ 0x41A4,		ebml_type_string,	4,	0x41E4,		}, // Segment\Tracks\TrackEntry\BlockAdditionMapping\BlockAddIDName
	{ 0x41E7,		ebml_type_uint,		4,	0x41E4,		}, // Segment\Tracks\TrackEntry\BlockAdditionMapping\BlockAddIDType
	{ 0x41ED,		ebml_type_binary,	4,	0x41E4,		}, // Segment\Tracks\TrackEntry\BlockAdditionMapping\BlockAddIDExtraData
	{ 0x536E,		ebml_type_utf8,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\Name
	{ 0x22B59C,		ebml_type_string,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\Language, default eng
	{ 0x22B59D,		ebml_type_string,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\LanguageIETF
	{ 0x86,			ebml_type_string,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecID
	{ 0x63A2,		ebml_type_binary,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecPrivate
	{ 0x258688,		ebml_type_utf8,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecName
	{ 0x7446,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\AttachmentLink
	{ 0x3A9697,		ebml_type_utf8,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecSettings
	{ 0x3B4040,		ebml_type_string,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecInfoURL [mult]
	{ 0x26B240,		ebml_type_string,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecDownloadURL [mult]
	{ 0xAA,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecDecodeAll
	{ 0x6FAB,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrackOverlay [mult]
	{ 0x56AA,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\CodecDelay
	{ 0x56BB,		ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\SeekPreRoll
	{ 0x6624,		ebml_type_master,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrackTranslate [mult]
	{ 0x66FC,		ebml_type_uint,		4,	0x6624,		}, // Segment\Tracks\TrackEntry\TrackTranslate\TrackTranslateEditionUID [mult]
	{ 0x66BF,		ebml_type_uint,		4,	0x6624,		}, // Segment\Tracks\TrackEntry\TrackTranslate\TrackTranslateCodec
	{ 0x66A5,		ebml_type_binary,	4,	0x6624,		}, // Segment\Tracks\TrackEntry\TrackTranslate\TrackTranslateTrackID
	{ 0xE0,			ebml_type_master,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\Video
	{ 0x9A,			ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\FlagInterlaced, default 0
	{ 0x9D,			ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\FieldOrder, default 2
	{ 0x53B8,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\StereoMode, default 0
	{ 0x53C0,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\AlphaMode, default 0
	{ 0x53B9,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\OldStereoMode
	{ 0xB0,			ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\PixelWidth
	{ 0xBA,			ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\PixelHeight
	{ 0x54AA,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\PixelCropBottom, default 0
	{ 0x54BB,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\PixelCropTop, default 0
	{ 0x54CC,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\PixelCropLeft, default 0
	{ 0x54DD,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\PixelCropRight, default 0
	{ 0x54B0,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\DisplayWidth
	{ 0x54BA,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\DisplayHeight
	{ 0x54B2,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\DisplayUnit, default 0
	{ 0x54B3,		ebml_type_uint,		4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\AspectRatioType, default 0
	{ 0x2EB524,		ebml_type_binary,	4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\ColourSpace
	{ 0x2FB523,		ebml_type_float,	4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\GammaValue
	{ 0x2383E3,		ebml_type_float,	4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\FrameRate
	{ 0x55B0,		ebml_type_master,	4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\Colour
	{ 0x55B1,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MatrixCoefficients, default 2
	{ 0x55B2,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\BitsPerChannel, default 0
	{ 0x55B3,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\ChromaSubsamplingHorz
	{ 0x55B4,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\ChromaSubsamplingVert
	{ 0x55B5,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\CbSubsamplingHorz
	{ 0x55B6,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\CbSubsamplingVert
	{ 0x55B7,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\ChromaSitingHorz, default 0
	{ 0x55B8,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\ChromaSitingVert, default 0
	{ 0x55B9,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\Range, default 0
	{ 0x55BA,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\TransferCharacteristics, default 2
	{ 0x55BB,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\Primaries, default 2
	{ 0x55BC,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MaxCLL
	{ 0x55BD,		ebml_type_uint,		5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MaxFALL
	{ 0x55D0,		ebml_type_master,	5,	0x55B0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata
	{ 0x55D1,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\PrimaryRChromaticityX
	{ 0x55D2,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\PrimaryRChromaticityY
	{ 0x55D3,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\PrimaryGChromaticityX
	{ 0x55D4,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\PrimaryGChromaticityY
	{ 0x55D5,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\PrimaryBChromaticityX
	{ 0x55D6,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\PrimaryBChromaticityY
	{ 0x55D7,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\WhitePointChromaticityX
	{ 0x55D8,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\WhitePointChromaticityY
	{ 0x55D9,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\LuminanceMax
	{ 0x55DA,		ebml_type_float,	6,	0x55D0,		}, // Segment\Tracks\TrackEntry\Video\Colour\MasteringMetadata\LuminanceMin
	{ 0x7670,		ebml_type_master,	4,	0xE0,		}, // Segment\Tracks\TrackEntry\Video\Projection
	{ 0x7671,		ebml_type_uint,		5,	0x7670,		}, // Segment\Tracks\TrackEntry\Video\ProjectionType
	{ 0x7672,		ebml_type_binary,	5,	0x7670,		}, // Segment\Tracks\TrackEntry\Video\ProjectionPrivate
	{ 0x7673,		ebml_type_float,	5,	0x7670,		}, // Segment\Tracks\TrackEntry\Video\ProjectionPoseYaw
	{ 0x7674,		ebml_type_float,	5,	0x7670,		}, // Segment\Tracks\TrackEntry\Video\ProjectionPosePitch
	{ 0x7675,		ebml_type_float,	5,	0x7670,		}, // Segment\Tracks\TrackEntry\Video\ProjectionPoseRoll
	{ 0xE1,			ebml_type_master,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\Audio
	{ 0xB5,			ebml_type_float,	4,	0xE1,		}, // Segment\Tracks\TrackEntry\Audio\SamplingFrequency, default 8000
	{ 0x78B5,		ebml_type_float,	4,	0xE1,		}, // Segment\Tracks\TrackEntry\Audio\OutputSamplingFrequency
	{ 0x9F,			ebml_type_uint,		4,	0xE1,		}, // Segment\Tracks\TrackEntry\Audio\Channels, default 1
	{ 0x7D7B,		ebml_type_binary,	4,	0xE1,		}, // Segment\Tracks\TrackEntry\Audio\ChannelPositions
	{ 0x6264,		ebml_type_uint,		4,	0xE1,		}, // Segment\Tracks\TrackEntry\Audio\BitDepth
	{ 0xE2,			ebml_type_master,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrackOperation
	{ 0xE3,			ebml_type_master,	4,	0xE2,		}, // Segment\Tracks\TrackEntry\TrackOperation\TrackCombinePlanes
	{ 0xE4,			ebml_type_master,	5,	0xE3,		}, // Segment\Tracks\TrackEntry\TrackOperation\TrackCombinePlanes\TrackPlane [mult]
	{ 0xE5,			ebml_type_uint,		6,	0xE4,		}, // Segment\Tracks\TrackEntry\TrackOperation\TrackCombinePlanes\TrackPlane\TrackPlaneUID
	{ 0xE6,			ebml_type_uint,		6,	0xE4,		}, // Segment\Tracks\TrackEntry\TrackOperation\TrackCombinePlanes\TrackPlane\TrackPlaneType
	{ 0xE9,			ebml_type_master,	4,	0xE2,		}, // Segment\Tracks\TrackEntry\TrackOperation\TrackJoinBlocks
	{ 0xED,			ebml_type_uint,		5,	0xE9,		}, // Segment\Tracks\TrackEntry\TrackOperation\TrackJoinBlocks\TrackJoinUID
	{ 0xC0,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrickTrackUID // 	DivX trick track extensions
	{ 0xC1,			ebml_type_binary,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrickTrackSegmentUID // 	DivX trick track extensions
	{ 0xC6,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrickTrackFlag // 	DivX trick track extensions
	{ 0xC7,			ebml_type_uint,		3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrickMasterTrackUID // 	DivX trick track extensions
	{ 0xC4,			ebml_type_binary,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\TrickMasterTrackSegmentUID // 	DivX trick track extensions
	{ 0x6D80,		ebml_type_master,	3,	0xAE,		}, // Segment\Tracks\TrackEntry\ContentEncodings
	{ 0x6240,		ebml_type_master,	4,	0x6D80,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding [mult]
	{ 0x5031,		ebml_type_uint,		5,	0x6240,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncodingOrder, default 0
	{ 0x5032,		ebml_type_uint,		5,	0x6240,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncodingScope, default 1
	{ 0x5033,		ebml_type_uint,		5,	0x6240,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncodingType, default 0
	{ 0x5034,		ebml_type_master,	5,	0x6240,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentCompression
	{ 0x4254,		ebml_type_uint,		6,	0x5034,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentCompression\ContentCompAlgo, default 0
	{ 0x4255,		ebml_type_binary,	6,	0x5034,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentCompression\ContentCompSettings
	{ 0x5035,		ebml_type_master,	5,	0x6240,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption
	{ 0x47E1,		ebml_type_uint,		6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentEncAlgo, default 0
	{ 0x47E2,		ebml_type_binary,	6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentEncKeyID
	{ 0x47E7,		ebml_type_master,	6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentEncAESSettings
	{ 0x47E8,		ebml_type_uint,		7,	0x47E7,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentEncAESSettings\AESSettingsCipherMode
	{ 0x47E3,		ebml_type_binary,	6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentSignature
	{ 0x47E4,		ebml_type_binary,	6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentSigKeyID
	{ 0x47E5,		ebml_type_uint,		6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentSigAlgo
	{ 0x47E6,		ebml_type_uint,		6,	0x5035,		}, // Segment\Tracks\TrackEntry\ContentEncodings\ContentEncoding\ContentEncryption\ContentSigHashAlgo

	// Cueing Data
	{ 0x1C53BB6B,	ebml_type_master,	1,	0x18538067, }, // Segment\Cues
	{ 0xBB,			ebml_type_master,	2,	0x1C53BB6B, }, // Segment\Cues\CuePoint [mult]
	{ 0xB3,			ebml_type_uint,		3,	0xBB, }, // Segment\Cues\CuePoint\CueTime
	{ 0xB7,			ebml_type_master,	3,	0xBB, }, // Segment\Cues\CuePoint\CueTrackPositions [mult]
	{ 0xF7,			ebml_type_uint,		4,	0xB7, }, // Segment\Cues\CuePoint\CueTrackPositions\CueTrack
	{ 0xF1,			ebml_type_uint,		4,	0xB7, }, // Segment\Cues\CuePoint\CueTrackPositions\CueClusterPosition
	{ 0xF0,			ebml_type_uint,		4,	0xB7, }, // Segment\Cues\CuePoint\CueTrackPositions\CueRelativePosition
	{ 0xB2,			ebml_type_uint,		4,	0xB7, }, // Segment\Cues\CuePoint\CueTrackPositions\CueDuration
	{ 0x5378,		ebml_type_uint,		4,	0xB7, }, // Segment\Cues\CuePoint\CueTrackPositions\CueBlockNumber, default 1
	{ 0xEA,			ebml_type_uint,		4,	0xB7, }, // Segment\Cues\CuePoint\CueTrackPositions\CueCodecState, default 0
	{ 0xDB,			ebml_type_master,	4,	0xB7, }, // Segment\Cues\CuePoint\CueTrackPositions\CueReference [mult]
	{ 0x96,			ebml_type_uint,		5,	0xDB, }, // Segment\Cues\CuePoint\CueTrackPositions\CueReference\CueRefTime
	{ 0x97,			ebml_type_uint,		5,	0xDB, }, // Segment\Cues\CuePoint\CueTrackPositions\CueReference\CueRefCluster
	{ 0x535F,		ebml_type_uint,		5,	0xDB, }, // Segment\Cues\CuePoint\CueTrackPositions\CueReference\CueRefNumber, default 1
	{ 0xEB,			ebml_type_uint,		5,	0xDB, }, // Segment\Cues\CuePoint\CueTrackPositions\CueReference\CueRefCodecState, default 0

	// Attachment
	{ 0x1941A469,	ebml_type_master,	1,	0x18538067, }, // Segment\Attachments
	{ 0x61A7,		ebml_type_master,	2,	0x1941A469, }, // Segment\Attachments\AttachedFile [mult]
	{ 0x467E,		ebml_type_utf8,		3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileDescription
	{ 0x466E,		ebml_type_utf8,		3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileName
	{ 0x4660,		ebml_type_string,	3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileMimeType
	{ 0x465C,		ebml_type_binary,	3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileData
	{ 0x46AE,		ebml_type_uint,		3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileUID
	{ 0x4675,		ebml_type_binary,	3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileReferral
	{ 0x4661,		ebml_type_uint,		3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileUsedStartTime
	{ 0x4662,		ebml_type_uint,		3,	0x61A7, }, // Segment\Attachments\AttachedFile\FileUsedEndTime

	// Chapters
	{ 0x1043A770,	ebml_type_master,	1,	0x18538067, }, // Segment\Chapters
	{ 0x45B9,		ebml_type_master,	2,	0x1043A770, }, // Segment\Chapters\EditionEntry [mult]
	{ 0x45BC,		ebml_type_uint,		3,	0x45B9,		}, // Segment\Chapters\EditionEntry\EditionUID
	{ 0x45BD,		ebml_type_uint,		3,	0x45B9,		}, // Segment\Chapters\EditionEntry\EditionFlagHidden, default 0
	{ 0x45DB,		ebml_type_uint,		3,	0x45B9,		}, // Segment\Chapters\EditionEntry\EditionFlagDefault, default 0
	{ 0x45DD,		ebml_type_uint,		3,	0x45B9,		}, // Segment\Chapters\EditionEntry\EditionFlagOrdered, default 0
	{ 0xB6,			ebml_type_master,	3,	0x45B9,		}, // Segment\Chapters\EditionEntry\ChapterAtom [mult]
	{ 0x73C4,		ebml_type_uint,		4,	0xB6,		}, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterUID
	{ 0x5654,		ebml_type_utf8,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterStringUID
	{ 0x91,			ebml_type_uint,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterTimeStart
	{ 0x92,			ebml_type_uint,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterTimeEnd
	{ 0x98,			ebml_type_uint,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterFlagHidden, default 0
	{ 0x4598,		ebml_type_uint,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterFlagEnabled, default 1
	{ 0x6E67,		ebml_type_binary,	4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterSegmentUID
	{ 0x6EBC,		ebml_type_uint,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterSegmentEditionUID
	{ 0x63C3,		ebml_type_uint,		4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterPhysicalEquiv
	{ 0x8F,			ebml_type_master,	4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterTrack
	{ 0x89,			ebml_type_uint,		5,	0x8F, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterTrack\ChapterTrackUID [mult]
	{ 0x80,			ebml_type_master,	4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterDisplay [mult]
	{ 0x85,			ebml_type_utf8,		5,	0x80, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterDisplay\ChapString
	{ 0x437C,		ebml_type_string,	5,	0x80, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterDisplay\ChapLanguage [mult], default eng
	{ 0x437D,		ebml_type_string,	5,	0x80, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterDisplay\ChapLanguageIETF [mult]
	{ 0x437E,		ebml_type_string,	5,	0x80, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapterDisplay\ChapCountry [mult]
	{ 0x6944,		ebml_type_master,	4,	0xB6, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapProcess [mult]
	{ 0x6955,		ebml_type_uint,		5,	0x6944, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapProcess\ChapProcessCodecID
	{ 0x450D,		ebml_type_binary,	5,	0x6944, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapProcess\ChapProcessPrivate
	{ 0x6911,		ebml_type_master,	5,	0x6944, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapProcess\ChapProcessCommand [mult]
	{ 0x6922,		ebml_type_uint,		6,	0x6911, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapProcess\ChapProcessComman\ChapProcessTime
	{ 0x6933,		ebml_type_binary,	6,	0x6911, }, // Segment\Chapters\EditionEntry\ChapterAtom\ChapProcess\ChapProcessComman\ChapProcessData

	// Tagging
	{ 0x1254C367,	ebml_type_master,	1,	0x18538067, }, // Segment\Tags [mult]
	{ 0x7373,		ebml_type_master,	2,	0x1254C367, }, // Segment\Tags\Tag [mult]
	{ 0x63C0,		ebml_type_master,	3,	0x7373, }, // Segment\Tags\Tag\Targets
	{ 0x68CA,		ebml_type_uint,		4,	0x63C0, }, // Segment\Tags\Tag\Targets\TargetTypeValue, default 50
	{ 0x63CA,		ebml_type_string,	4,	0x63C0, }, // Segment\Tags\Tag\Targets\TargetType
	{ 0x63C5,		ebml_type_uint,		4,	0x63C0, }, // Segment\Tags\Tag\Targets\TagTrackUID [mult]
	{ 0x63C9,		ebml_type_uint,		4,	0x63C0, }, // Segment\Tags\Tag\Targets\TagEditionUID [mult]
	{ 0x63C4,		ebml_type_uint,		4,	0x63C0, }, // Segment\Tags\Tag\Targets\TagChapterUID [mult]
	{ 0x63C6,		ebml_type_uint,		4,	0x63C0, }, // Segment\Tags\Tag\Targets\TagAttachmentUID [mult]
	{ 0x67C8,		ebml_type_master,	3,	0x7373, }, // Segment\Tags\Tag\SimpleTag [mult]
	{ 0x45A3,		ebml_type_utf8,		4,	0x67C8, }, // Segment\Tags\Tag\SimpleTag\TagName
	{ 0x447A,		ebml_type_string,	4,	0x67C8, }, // Segment\Tags\Tag\SimpleTag\TagLanguage, default und
	{ 0x447B,		ebml_type_string,	4,	0x67C8, }, // Segment\Tags\Tag\SimpleTag\TagLanguageIETF
	{ 0x4484,		ebml_type_uint,		4,	0x67C8, }, // Segment\Tags\Tag\SimpleTag\TagDefault, default 1
	{ 0x4487,		ebml_type_utf8,		4,	0x67C8, }, // Segment\Tags\Tag\SimpleTag\TagString
	{ 0x4485,		ebml_type_binary,	4,	0x67C8, }, // Segment\Tags\Tag\SimpleTag\TagBinary
};

static int webm_reader_open(webm_reader_t* reader)
{
	while (1)
	{
		reader->io.read(reader->param, 
	}
}

webm_reader_t* webm_reader_create(const struct webm_buffer_t* buffer, void* param)
{
	struct webm_reader_t* reader;
	reader = (struct webm_reader_t*)calloc(1, sizeof(*reader));
	if (NULL == reader)
		return NULL;

	memcpy(&reader->io, buffer, sizeof(reader->io));
	reader->param = param;

	if (0 != webm_reader_open(reader))
	{
		webm_reader_destroy(reader);
		return NULL;
	}

	return reader;
}

void webm_reader_destroy(webm_reader_t* reader)
{
	int i;
	//for (i = 0; i < reader->webm.track_count; i++)
	//	webm_free_track(reader->webm.tracks + i);
	if (reader->webm.tracks)
		free(reader->webm.tracks);
	free(reader);
}

int webm_reader_getinfo(webm_reader_t* reader, struct webm_reader_trackinfo_t* ontrack, void* param)
{
	return -1;
}

uint64_t webm_reader_getduration(webm_reader_t* reader)
{
	return 0;
}

int webm_reader_read(webm_reader_t* reader, void* buffer, size_t bytes, webm_reader_onread onread, void* param)
{
	return -1;
}

int webm_reader_seek(webm_reader_t* reader, int64_t* timestamp)
{
	return -1;
}
