#include "rtp-profile.h"

enum
{
	RTP_TYPE_UNKNOWN = 0,
	RTP_TYPE_AUDIO,
	RTP_TYPE_VIDEO,
	RTP_TYPE_SYSTEM,
};

static struct rtp_profile_t s_profiles[] = {
	// audio
	{ 0, RTP_TYPE_AUDIO,	1, 8000,	"PCMU" }, // G711 mu-law
	{ 1, RTP_TYPE_UNKNOWN,	1, 8000,	"FS-1016 CELP"	}, // reserved
	{ 2, RTP_TYPE_UNKNOWN,	1, 8000,	"G721" }, // reserved
	{ 3, RTP_TYPE_AUDIO,	1, 8000,	"GSM"  },
	{ 4, RTP_TYPE_AUDIO,	1, 8000,	"G723" },
	{ 5, RTP_TYPE_AUDIO,	1, 8000,	"DVI4" },
	{ 6, RTP_TYPE_AUDIO,	1, 16000,	"DVI4" },
	{ 7, RTP_TYPE_AUDIO,	1, 8000,	"LPC"  },
	{ 8, RTP_TYPE_AUDIO,	1, 8000,	"PCMA" }, // G711 A-law
	{ 9, RTP_TYPE_AUDIO,	1, 8000,	"G722" },
	{ 10,RTP_TYPE_AUDIO,	2, 44100,	"L16"  }, // PCM S16BE
	{ 11,RTP_TYPE_AUDIO,	1, 44100,	"L16"  }, // PCM S16BE
	{ 12,RTP_TYPE_AUDIO,	1, 8000,	"QCELP"},
	{ 13,RTP_TYPE_AUDIO,	1, 8000,	"CN"   },
	{ 14,RTP_TYPE_AUDIO,	2, 90000,	"MPA"  }, // MPEG-1/MPEG-2 audio 1/2 channels
	{ 15,RTP_TYPE_AUDIO,	1, 8000,	"G728" },
	{ 16,RTP_TYPE_AUDIO,	1, 11025,	"DVI4" },
	{ 17,RTP_TYPE_AUDIO,	1, 22050,	"DVI4" },
	{ 18,RTP_TYPE_AUDIO,	1, 8000,	"G729" },
	{ 19,RTP_TYPE_UNKNOWN,	0, 0,		"CN"   }, // reserved
	{ 20,RTP_TYPE_UNKNOWN,	0, 0,		""     }, // unassigned
	{ 21,RTP_TYPE_UNKNOWN,	0, 0,		""     }, // unassigned
	{ 22,RTP_TYPE_UNKNOWN,	0, 0,		""     }, // unassigned
	{ 23,RTP_TYPE_UNKNOWN,	0, 0,		""     }, // unassigned
	{ 24,RTP_TYPE_UNKNOWN,	0, 0,		""     }, // unassigned
	//{ 0, "G726-40", 8000,	1 },
	//{ 0, "G726-32", 8000,	1 },
	//{ 0, "G726-24", 8000,	1 },
	//{ 0, "G726-16", 8000,	1 },
	//{ 0, "G729-D",  8000,	1 },
	//{ 0, "G729-E",  8000,	1 },
	//{ 0, "GSM-EFR", 8000,	1 },
	//{ 0, "L8",      var,	1 },

	// video
	{ 25,RTP_TYPE_VIDEO,	0, 90000,	"CELB" }, // SUN CELL-B
	{ 26,RTP_TYPE_VIDEO,	0, 90000,	"JPEG" }, // MJPEG
	{ 27,RTP_TYPE_UNKNOWN,	0, 0,		""     }, // unassigned
	{ 28,RTP_TYPE_VIDEO,	0, 90000,	"nv"   },
	{ 29,RTP_TYPE_UNKNOWN,	0, 0,		""     }, // unassigned
	{ 30,RTP_TYPE_UNKNOWN,	0, 0,		""     }, // unassigned
	{ 31,RTP_TYPE_VIDEO,	0, 90000,	"H261" },
	{ 32,RTP_TYPE_VIDEO,	0, 90000,	"MPV"  }, // MPEG-1/MPEG-2 video
	{ 33,RTP_TYPE_SYSTEM,	0, 90000,	"MP2T" }, // MPEG-2 TS
	{ 34,RTP_TYPE_VIDEO,	0, 90000,	"H263" },
	//{ 0, "H263-1998",90000,	0 },

	// 35-71 unassigned
	// 72-76 reserved
	// 77-95 unassigned
	// 96-127 dynamic
	//{ 96,RTP_TYPE_VIDEO,	0, 90000,	"MPG4" }, // RFC3640 RTP Payload Format for Transport of MPEG-4 Elementary Streams
	//{ 97,RTP_TYPE_SYSTEM,	0, 90000,	"MP2P" }, // RFC3555 4.2.11 Registration of MIME media type video/MP2P
	//{ 98,RTP_TYPE_VIDEO,	0, 90000,	"H264" }, // RFC6184 RTP Payload Format for H.264 Video
};

const struct rtp_profile_t* rtp_profile_find(int payload)
{
	if (payload < 0 || payload >= 35)
		return 0;

	return RTP_TYPE_UNKNOWN == s_profiles[payload].avtype ? 0 : &s_profiles[payload];
}
