#ifndef _rtp_profile_h_
#define _rtp_profile_h_

/// https://en.wikipedia.org/wiki/RTP_audio_video_profile
/// RFC3551 6. Payload Type Definitions (p28)
struct rtp_profile_t
{
	int pt;
	const char* encoding;
	int clock;
	int channels;
};

/***
{
	// audio
	{ 0, "PCMU",	8000,	1 }, // G711 mu-law
	{ 1, "",		0,		0 }, // reserved
	{ 2, "",		0,		0 }, // reserved
	{ 3, "GSM",		8000,	1 },
	{ 4, "G723",	8000,	1 },
	{ 5, "DVI4",	8000,	1 },
	{ 6, "DVI4",	16000,	1 },
	{ 7, "LPC",		8000,	1 },
	{ 8, "PCMA",	8000,	1 }, // G711 A-law
	{ 9, "G722",	8000,	1 },
	{10, "L16",		44100,	2 },
	{11, "L16",		44100,	1 },
	{12, "QCELP",	8000,	1 },
	{13, "CN",		8000,	1 },
	{14, "MPA",		90000,	0 }, // MPEG-1/MPEG-2 audio
	{15, "G728",	8000,	1 },
	{16, "DVI4",	11025,	1 },
	{17, "DVI4",	22050,	1 },
	{18, "G729",	8000,	1 },
	{19, "",		0,		0 }, // reserved
	{20, "",		0,		0 }, // unassigned
	{21, "",		0,		0 }, // unassigned
	{22, "",		0,		0 }, // unassigned
	{23, "",		0,		0 }, // unassigned
	//{ 0, "G726-40", 8000,	1 },
	//{ 0, "G726-32", 8000,	1 },
	//{ 0, "G726-24", 8000,	1 },
	//{ 0, "G726-16", 8000,	1 },
	//{ 0, "G729-D",  8000,	1 },
	//{ 0, "G729-E",  8000,	1 },
	//{ 0, "GSM-EFR", 8000,	1 },
	//{ 0, "L8",      var,	1 },

	// video
	{24, "",		0,		0 }, // unassigned
	{25, "CelB",	90000,	0 }, // SUN CELL-B
	{26, "JPEG",	90000,	0 },
	{27, "",		0,		0 }, // unassigned
	{28, "nv",		90000,	0 },
	{29, "",		0,		0 }, // unassigned
	{30, "",		0,		0 }, // unassigned
	{31, "H261",	90000,	0 },
	{32, "MPV",		90000,	0 }, // MPEG-1/MPEG-2 video
	{33, "MP2T",	90000,	0 }, // MPEG-2 TS
	{34, "H263",	90000,	0 },
	//{ 0, "H263-1998",90000,	0 },

	// 35-71 unassigned
	// 72-76 reserved
	// 77-95 unassigned
	// 96-127 dynamic
	{96, "MPG4",	90000,  0 }, // RFC3640 RTP Payload Format for Transport of MPEG-4 Elementary Streams
	{97, "MP2P",	90000,  0 }, // RFC3555 4.2.11 Registration of MIME media type video/MP2P
	{98, "H264",	90000,	0 }, // RFC6184 RTP Payload Format for H.264 Video
};
***/

enum
{
	RTP_PAYLOAD_G711U		= 0,  // ITU-T G.711 PCM µ-Law audio 64 kbit/s (rfc3551)
	RTP_PAYLOAD_G722		= 9,  // ITU-T G.722 audio 64 kbit/s (rfc3551)
	RTP_PAYLOAD_G729		= 18, // ITU-T G.729 and G.729a audio 8 kbit/s (rfc3551)
	RTP_PAYLOAD_MPA			= 14, // MPEG-1/MPEG-2 audio (rfc2250)
	RTP_PAYLOAD_MP4A		= 99, // MP4A-LATM MPEG-4 Audio (rfc6416)

	RTP_PAYLOAD_JPEG		= 26, // JPEG video (rfc2435)
	RTP_PAYLOAD_MPV			= 32, // MPEG-1 and MPEG-2 video (rfc2250)
	RTP_PAYLOAD_MP2T		= 33, // MPEG-2 transport stream (rfc2250)
	RTP_PAYLOAD_H263		= 34, // H.263 video, first version (1996) (rfc2190)

	RTP_PAYLOAD_MP4V		= 96, // MP4V-ES MPEG-4 Visual (rfc6416)
	RTP_PAYLOAD_H264		= 97, // H.264 video (MPEG-4 Part 10) (rfc6184)
	RTP_PAYLOAD_MP2P		= 98, // MPEG-2 Program Streams video (rfc2250)
	RTP_PAYLOAD_MP4ES		= 100, // MPEG4-generic audio/video MPEG-4 Elementary Streams (rfc3640)

};

#endif /* _rtp_profile_h_ */
