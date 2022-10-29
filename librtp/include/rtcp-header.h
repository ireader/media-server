#ifndef _rtcp_header_h_ 
#define _rtcp_header_h_

#include <stdint.h>

#define RTCP_V(v)	((v >> 30) & 0x03) // rtcp version
#define RTCP_P(v)	((v >> 29) & 0x01) // rtcp padding
#define RTCP_RC(v)	((v >> 24) & 0x1F) // rtcp reception report count
#define RTCP_PT(v)	((v >> 16) & 0xFF) // rtcp packet type
#define RTCP_LEN(v)	(v & 0xFFFF) // rtcp packet length

// https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml
enum rtcp_type_t
{
    RTCP_FIR    = 192, // RFC2032, Reserved (Historic-FIR)
    RTCP_NACK   = 193, // RFC2032, Reserved (Historic-NACK)
    RTCP_SMPTETC= 194, // RFC5484
    RTCP_IJ     = 195, // RFC5450
    
	RTCP_SR		= 200,
	RTCP_RR		= 201,
	RTCP_SDES	= 202,
	RTCP_BYE	= 203,
	RTCP_APP	= 204,
    
    RTCP_RTPFB  = 205, // RFC4585
    RTCP_PSFB   = 206, // RFC4585
    RTCP_XR     = 207, // RFC3611
    RTCP_AVB    = 208,
    RTCP_RSI    = 209, // RFC5760
    RTCP_TOKEN  = 210, // RFC6284
	RTCP_IDMS	= 211, // RFC7272
	RTCP_RGRS	= 212, // RFC8861

	RTCP_LIMIT  = 223, // RFC5761 RTCP packet types in the ranges 1-191 and 224-254 SHOULD only be used when other values have been exhausted.
};

// https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml#rtp-parameters-5
enum rtcp_sdes_type_t
{
	RTCP_SDES_END		= 0, // RFC3550
	RTCP_SDES_CNAME		= 1, // RFC3550
	RTCP_SDES_NAME		= 2, // RFC3550
	RTCP_SDES_EMAIL		= 3, // RFC3550
	RTCP_SDES_PHONE		= 4, // RFC3550
	RTCP_SDES_LOC		= 5, // RFC3550
	RTCP_SDES_TOOL		= 6, // RFC3550
	RTCP_SDES_NOTE		= 7, // RFC3550
	RTCP_SDES_PRIVATE	= 8, // RFC3550
	RTCP_SDES_CADDR		= 9, // H.323 callable address
	RTCP_SDES_APSI		= 10, // RFC6776
	RTCP_SDES_RGRP		= 11, // RFC8861
	RTCP_SDES_RID		= 12, // RFC8852
	RTCP_SDES_RRID		= 13, // RFC8852
	RTCP_SDES_CCID		= 14, // RFC8849
	RTCP_SDES_MID		= 15, // RFC8843
};

// https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml#rtp-parameters-8
enum rtcp_rtpfb_type_t
{
	RTCP_RTPFB_NACK		= 1, // RFC4585, Generic NACK
	RTCP_RTPFB_TMMBR	= 3, // RFC5104, Temporary Maximum Media Stream Bit Rate Request (TMMBR)
	RTCP_RTPFB_TMMBN	= 4, // RFC5104, Temporary Maximum Media Stream Bit Rate Notification (TMMBN)
	RTCP_RTPFB_SRREQ	= 5, // RFC6051, RTCP Rapid Resynchronisation Request(RTCP-SR-REQ)
	RTCP_RTPFB_RAMS		= 6, // RFC6285, Rapid Acquisition of Multicast Sessions
	RTCP_RTPFB_TLLEI	= 7, // RFC6642, Transport-Layer Third-Party Loss Early Indication
	RTCP_RTPFB_ECN		= 8, // RFC6679, RTCP ECN Feedback
	RTCP_RTPFB_PS		= 9, // RFC7728, Media Pause/Resume
	RTCP_RTPFB_DBI		= 10, // 3GPP TS 26.114 v16.3.0, Delay Budget Information (DBI)
	RTCP_RTPFB_CCFB		= 11, // RFC8888, RTP Congestion Control Feedback
	RTCP_RTPFB_TCC01	= 15, // draft-holmer-rmcat-transport-wide-cc-extensions-01

	RTCP_RTPFB_EXT		= 31,
};

// https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml#rtp-parameters-9
enum rtcp_psfb_type_t
{
	RTCP_PSFB_PLI	= 1, // RFC4585, Picture Loss Indication (PLI)
	RTCP_PSFB_SLI	= 2, // RFC4585, Slice Loss Indication (SLI)
	RTCP_PSFB_RPSI	= 3, // RFC4585, Reference Picture Selection Indication (RPSI)
	RTCP_PSFB_FIR	= 4, // RFC5104, Full Intra Request (FIR)
	RTCP_PSFB_TSTR	= 5, // RFC5104, Temporal-Spatial Trade-off Request
	RTCP_PSFB_TSTN	= 6, // RFC5104, Temporal-Spatial Trade-off Notification
	RTCP_PSFB_VBCM	= 7, // RFC5104, Video Back Channel Message
	RTCP_PSFB_PSLEI	= 8, // RFC6642, Payload-Specific Third-Party Loss Early Indication
	RTCP_PSFB_ROI	= 9, // 3GPP TS 26.114 v16.3.0, Video region-of-interest (ROI)
	RTCP_PSFB_LRR	= 10, // RFC-ietf-avtext-lrr-07, Layer Refresh Request Command
	RTCP_PSFB_AFB	= 15, // RFC4585, Application layer FB (AFB) message
	RTCP_PSFB_REMB  = 15, // https://datatracker.ietf.org/doc/html/draft-alvestrand-rmcat-remb-03#section-2.2

	RTCP_PSFB_EXT	= 31,
};

// https://www.iana.org/assignments/rtcp-xr-block-types/rtcp-xr-block-types.xhtml
enum rtcp_xr_type_t
{
	RTCP_XR_LRLE	= 1, // RFC3611, Loss RLE Report Block
	RTCP_XR_DRLE	= 2, // RFC3611, Duplicate RLE Report Block
	RTCP_XR_PRT		= 3, // RFC3611, Packet Receipt Times Report Block
	RTCP_XR_RRT		= 4, // RFC3611, Receiver Reference Time Report Block
	RTCP_XR_DLRR	= 5, // RFC3611, DLRR Report Block
	RTCP_XR_SS		= 6, // RFC3611, Statistics Summary Report Block
	RTCP_XR_VM		= 7, // RFC3611, VoIP Metrics Report Block
	RTCP_XR_RTCP	= 8, // RFC5093, RTCP XR
	
	RTCP_XR_IDMS	= 12, // RFC7272, IDMS Report Block
	RTCP_XR_ECN		= 13, // RFC6679, ECN Summary Report

	RTCP_XR_DISRLE	= 25, // RFC7097, DRLE (Discard RLE Report)
	RTCP_XR_BDR		= 26, // RFC7243, BDR (Bytes Discarded Report)
	RTCP_XR_RFISD	= 27, // RFC7244, RFISD (RTP Flows Initial Synchronization Delay)
	RTCP_XR_RFSO	= 28, // RFC7244, RFSO (RTP Flows Synchronization Offset Metrics Block)
};

typedef struct _rtcp_header_t
{
	uint32_t v:2;		// version
	uint32_t p:1;		// padding
	uint32_t rc:5;		// reception report count
	uint32_t pt:8;		// packet type
	uint32_t length:16; /* pkt len in words, w/o this word */
} rtcp_header_t;

typedef struct _rtcp_sr_t // sender report
{
	uint32_t ssrc;
	uint32_t ntpmsw; // ntp timestamp MSW(in second)
	uint32_t ntplsw; // ntp timestamp LSW(in picosecond)
	uint32_t rtpts;	 // rtp timestamp
	uint32_t spc;	 // sender packet count
	uint32_t soc;	 // sender octet count
} rtcp_sr_t;

typedef struct _rtcp_rr_t // receiver report
{
	uint32_t ssrc;
} rtcp_rr_t;

typedef struct _rtcp_rb_t // report block
{
	uint32_t ssrc;
	uint32_t fraction:8; // fraction lost
	uint32_t cumulative:24; // cumulative number of packets lost
	uint32_t exthsn; // extended highest sequence number received
	uint32_t jitter; // interarrival jitter
	uint32_t lsr; // last SR
	uint32_t dlsr; // delay since last SR
} rtcp_rb_t;

// source description RTCP packet
typedef struct _rtcp_sdes_item_t
{
	uint8_t pt; // chunk type
	uint8_t len;
	uint8_t *data;
} rtcp_sdes_item_t;

typedef struct _rtcp_bye_t
{
	const void* reason;
	int bytes; // reason length
} rtcp_bye_t;

typedef struct _rtcp_app_t
{
	uint8_t subtype;
	char name[4];
	void* data;
	int bytes; // data length
} rtcp_app_t;


// Slice Loss Indication (SLI)
typedef struct _rtcp_sli_t
{
	uint32_t first : 13; // The macroblock (MB) address of the first lost macroblock.
	uint32_t number : 13; // The number of lost macroblocks, in scan order
	uint32_t picture_id : 6;
} rtcp_sli_t;


// Full Intra Request (FIR)
typedef struct _rtcp_fir_t
{
	uint32_t ssrc;
	uint32_t sn : 8; // Command sequence number
	uint32_t reserved : 19;
	uint32_t index : 5; // for TSTR
} rtcp_fir_t;


// Video Back Channel Message (VBCM)
typedef struct _rtcp_vbcm_t
{
	uint32_t ssrc;
	uint32_t sn : 8; // Command sequence number
	uint32_t reserved : 1;
	uint32_t pt : 7;
	uint32_t len: 16;
	void* payload;
} rtcp_vbcm_t;


// Layer Refresh Request
typedef struct _rtcp_lrr_t
{
	uint32_t ssrc;

	uint32_t sn : 8; // Command sequence number
	uint32_t c : 1;
	uint32_t payload : 7;
	uint32_t reserved : 16; // Reserved 

	uint32_t res1 : 5; // reserved 
	uint32_t ttid : 3; // Target Temporal Layer ID (TTID) (3 bits)
	uint32_t tlid : 8; // Target Layer ID (TLID) (8 bits)
	uint32_t res2 : 5; // reserved 
	uint32_t ctid : 3; // Current Temporal Layer ID (CTID) (3 bits)
	uint32_t clid : 8; // Current Layer ID (CLID) (8 bits)
} rtcp_lrr_t;


// Generic NACK
typedef struct _rtcp_nack_t
{
	uint16_t pid; // Packet ID (PID)
	uint16_t blp; // bitmask of following lost packets (BLP)
} rtcp_nack_t;


// Temporary Maximum Media Stream Bit Rate Request (TMMBR)
typedef struct _rtcp_tmmbr_t
{
	uint32_t ssrc;
	uint32_t exp : 6; // MxTBR Exp (6 bits)
	uint32_t mantissa : 17; // MxTBR Mantissa (17 bits)
	uint32_t overhead : 9; // Measured Overhead (9 bits)

	// maximum total media bit rate(MxTBR)
	// MxTBR = mantissa * 2^exp
} rtcp_tmmbr_t;


// RTP/AVPF Transport-Layer ECN Feedback Packet(ECN)
typedef struct _rtcp_ecn_t
{
	uint32_t ext_highest_seq; // 32-bit extended highest sequence number received
	uint32_t ect[2]; // The 32-bit cumulative number of RTP packets received from this SSRC.
	uint16_t ect_ce_counter;
	uint16_t not_ect_counter;
	uint16_t lost_packets_counter;
	uint16_t duplication_counter;
} rtcp_ecn_t;

typedef struct _rtcp_remb_t
{
	uint32_t ssrc;
	uint32_t exp : 6; // BR Exp (6 bits)
	uint32_t mantissa : 18; // BR Mantissa (18 bits) in bps

	// maximum total media bit rate(MxTBR)
	// MxTBR = mantissa * 2^exp
} rtcp_remb_t;

// RTP Congestion Control Feedback
typedef struct _rtcp_ccfb_t
{
	uint32_t seq : 16;
	uint32_t received : 1;
	uint32_t ecn : 2; // 00 if not received or if ECN is not used
	//uint32_t ato : 13;

	int16_t ato; // ms * 1024, Arrival time offset (ATO, 13 bits) & (TCC-01 16 bits)
} rtcp_ccfb_t;

// DLRR Report Block
typedef struct _rtcp_dlrr_t
{
	uint32_t ssrc;
	uint32_t lrr;
	uint32_t dlrr;
} rtcp_dlrr_t;


typedef struct _rtcp_rtpfb_t
{
	uint32_t media; // media ssrc

	union
	{
		// RTCP_RTPFB | (RTCP_RTPFB_NACK << 8)
		// RTCP_RTPFB | (RTCP_RTPFB_TLLEI << 8)
		struct
		{
			rtcp_nack_t* nack;
			int count;
		} nack;

		// RTCP_RTPFB | (RTCP_RTPFB_TMMBR << 8)
		// RTCP_RTPFB | (RTCP_RTPFB_TMMBN << 8)
		struct
		{
			rtcp_tmmbr_t* tmmbr;
			int count;
		} tmmbr;

		// RTCP_RTPFB | (RTCP_RTPFB_CCFB << 8)
		// RTCP_RTPFB | (RTCP_RTPFB_TCC01 << 8)
		struct
		{
			rtcp_ccfb_t* ccfb;
			int count;

			int32_t timestamp;
			uint32_t begin : 16;
			uint32_t cc : 8; // TCC01 only

			uint32_t ssrc; // ccfb only
		} tcc01;

		// RTCP_RTPFB | (RTCP_RTPFB_PS << 8)
		struct
		{
			uint32_t target;
			uint32_t cmd : 8;
			uint32_t len : 8;
			uint32_t id  : 16;
			void* payload;
		} ps;

		// RTCP_RTPFB | (RTCP_RTPFB_ECN << 8)
		rtcp_ecn_t ecn;

		// RTCP_RTPFB | (RTCP_RTPFB_DBI << 8)
		struct
		{
			uint32_t delay : 16;
			uint32_t s : 1;
			uint32_t q : 1;
		} dbi;
	} u;
} rtcp_rtpfb_t;

typedef struct _rtcp_psfb_t
{
	uint32_t media; // media ssrc

	union
	{
		// RTCP_PSFB | (RTCP_PSFB_PLI << 8)

		// RTCP_PSFB | (RTCP_PSFB_SLI << 8)
		struct
		{
			rtcp_sli_t* sli;
			int count;
		} sli;

		// RTCP_PSFB | (RTCP_PSFB_FIR << 8)
		// RTCP_PSFB | (RTCP_PSFB_TSTR << 8)
		// RTCP_PSFB | (RTCP_PSFB_TSTN << 8)
		struct
		{
			rtcp_fir_t* fir;
			int count;
		} fir;

		// RTCP_PSFB | (RTCP_PSFB_VBCM << 8)
		struct
		{
			uint8_t pt;
			uint32_t len; // length in bit
			void* payload;
		} rpsi;
		
		// RTCP_PSFB | (RTCP_PSFB_VBCM << 8)
		rtcp_vbcm_t vbcm;

		// RTCP_PSFB | (RTCP_PSFB_PSLEI << 8)
		struct
		{
			uint32_t* ssrc;
			int count;
		} pslei;

		// RTCP_PSFB | (RTCP_PSFB_LRR << 8)
		struct
		{
			rtcp_lrr_t* lrr;
			int count;
		} lrr;

		// RTCP_PSFB | (RTCP_PSFB_AFB << 8)
		struct
		{
			rtcp_remb_t* remb;
			int count;
		} afb;
	} u;
} rtcp_psfb_t;

typedef struct _rtcp_xr_t
{
	union
	{
		// RTCP_XR | (RTCP_XR_LRLE << 8)
		// RTCP_XR | (RTCP_XR_DRLE << 8)
		struct
		{
			uint32_t source; // SSRC of source
			uint32_t begin : 16; // begin_seq
			uint32_t end : 16; // end_seq
			uint8_t* chunk;
			int count;
		} rle; // lrle/drle

		// RTCP_XR | (RTCP_XR_PRT << 8)
		struct
		{
			uint32_t source; // SSRC of source
			uint32_t begin : 16; // begin_seq
			uint32_t end : 16; // end_seq
			uint32_t* timestamp;
			int count;
		} prt;

		// RTCP_XR | (RTCP_XR_RRT << 8)
		uint64_t rrt;
		
		// RTCP_XR | (RTCP_XR_DLRR << 8)
		struct
		{
			rtcp_dlrr_t* dlrr;
			int count;
		} dlrr;

		// RTCP_XR | (RTCP_XR_ECN << 8)
		rtcp_ecn_t ecn;
	} u;
} rtcp_xr_t;

#endif /* !_rtcp_header_h_ */
