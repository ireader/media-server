#include "rtp-payload.h"
#include "rtp-profile.h"
#include "rtp-packet.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TS_PACKET_SIZE 188

#if defined(OS_WINDOWS)
#define strcasecmp _stricmp
#endif

struct rtp_payload_delegate_t
{
	struct rtp_payload_encode_t* encoder;
	struct rtp_payload_decode_t* decoder;
	void* packer;
};

/// @return 0-ok, <0-error
static int rtp_payload_find(int payload, const char* encoding, struct rtp_payload_delegate_t* codec);

void* rtp_payload_encode_create(int payload, const char* name, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam)
{
	int size;
	struct rtp_payload_delegate_t* ctx;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx)
	{
		size = rtp_packet_getsize();
		if (rtp_payload_find(payload, name, ctx) < 0
			|| NULL == (ctx->packer = ctx->encoder->create(size, (uint8_t)payload, seq, ssrc, handler, cbparam)))
		{
			free(ctx);
			return NULL;
		}
	}
	return ctx;
}

void rtp_payload_encode_destroy(void* encoder)
{
	struct rtp_payload_delegate_t* ctx;
	ctx = (struct rtp_payload_delegate_t*)encoder;
	ctx->encoder->destroy(ctx->packer);
	free(ctx);
}

void rtp_payload_encode_getinfo(void* encoder, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_payload_delegate_t* ctx;
	ctx = (struct rtp_payload_delegate_t*)encoder;
	ctx->encoder->get_info(ctx->packer, seq, timestamp);
}

int rtp_payload_encode_input(void* encoder, const void* data, int bytes, uint32_t timestamp)
{
	struct rtp_payload_delegate_t* ctx;
	ctx = (struct rtp_payload_delegate_t*)encoder;
	return ctx->encoder->input(ctx->packer, data, bytes, timestamp);
}

void* rtp_payload_decode_create(int payload, const char* name, struct rtp_payload_t *handler, void* cbparam)
{
	struct rtp_payload_delegate_t* ctx;
	ctx = calloc(1, sizeof(*ctx));
	if (ctx)
	{
		if (rtp_payload_find(payload, name, ctx) < 0
			|| NULL == (ctx->packer = ctx->decoder->create(handler, cbparam)))
		{
			free(ctx);
			return NULL;
		}
	}
	return ctx;
}

void rtp_payload_decode_destroy(void* decoder)
{
	struct rtp_payload_delegate_t* ctx;
	ctx = (struct rtp_payload_delegate_t*)decoder;
	ctx->decoder->destroy(ctx->packer);
	free(ctx);
}

int rtp_payload_decode_input(void* decoder, const void* packet, int bytes)
{
	struct rtp_payload_delegate_t* ctx;
	ctx = (struct rtp_payload_delegate_t*)decoder;
	return ctx->decoder->input(ctx->packer, packet, bytes);
}

// Default max packet size (1500, minus allowance for IP, UDP, UMTP headers)
// (Also, make it a multiple of 4 bytes, just in case that matters.)
//static int s_max_packet_size = 1456; // from Live555 MultiFrameRTPSink.cpp RTP_PAYLOAD_MAX_SIZE
//static size_t s_max_packet_size = 576; // UNIX Network Programming by W. Richard Stevens
static int s_max_packet_size = 1434; // from VLC

void rtp_packet_setsize(int bytes)
{
	s_max_packet_size = bytes < 564 ? 564 : bytes;
}

int rtp_packet_getsize()
{
	return s_max_packet_size;
}

static int rtp_payload_find(int payload, const char* encoding, struct rtp_payload_delegate_t* codec)
{
	assert(payload >= 0 && payload <= 127);
	if (payload >= 96 && encoding)
	{
		if (0 == strcasecmp(encoding, "H264"))
		{
			// H.264 video (MPEG-4 Part 10) (RFC 6184)
			codec->encoder = rtp_h264_encode();
			codec->decoder = rtp_h264_decode();
		}
		else if (0 == strcasecmp(encoding, "H265") || 0 == strcasecmp(encoding, "HEVC"))
		{
			// H.265 video (HEVC) (RFC 7798)
			codec->encoder = rtp_h265_encode();
			codec->decoder = rtp_h265_decode();
		}
		else if (0 == strcasecmp(encoding, "MP4V-ES"))
		{
			// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams
			// 5. RTP Packetization of MPEG-4 Visual Bitstreams (p8)
			// 7.1 Media Type Registration for MPEG-4 Audio/Visual Streams (p17)
			codec->encoder = rtp_mp4v_es_encode();
			codec->decoder = rtp_mp4v_es_decode();
		}
		else if (0 == strcasecmp(encoding, "MP4A-LATM"))
		{
			// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams
			// 6. RTP Packetization of MPEG-4 Audio Bitstreams (p15)
			// 7.3 Media Type Registration for MPEG-4 Audio (p21)
			codec->encoder = rtp_mp4a_latm_encode();
			codec->decoder = rtp_mp4a_latm_decode();
		}
		else if (0 == strcasecmp(encoding, "mpeg4-generic"))
		{
			/// RFC3640 RTP Payload Format for Transport of MPEG-4 Elementary Streams
			/// 4.1. MIME Type Registration (p27)
			codec->encoder = rtp_mpeg4_generic_encode();
			codec->decoder = rtp_mpeg4_generic_decode();
		}
		else if (0 == strcasecmp(encoding, "VP8"))
		{
			/// RFC7741 RTP Payload Format for VP8 Video
			/// 6.1. Media Type Definition (p21)
			codec->encoder = rtp_vp8_encode();
			codec->decoder = rtp_vp8_decode();
		}
		else if (0 == strcasecmp(encoding, "VP9"))
		{
			/// RTP Payload Format for VP9 Video draft-ietf-payload-vp9-03
			/// 6.1. Media Type Definition (p15)
			codec->encoder = rtp_vp9_encode();
			codec->decoder = rtp_vp9_decode();
		}
		else if (0 == strcasecmp(encoding, "AV1"))
		{
			/// https://aomediacodec.github.io/av1-rtp-spec/#7-payload-format-parameters
			codec->encoder = rtp_av1_encode();
			codec->decoder = rtp_av1_decode();
		}
        else if (0 == strcasecmp(encoding, "MP2P")) // MPEG-2 Program Streams video (RFC 2250)
        {
            codec->encoder = rtp_ts_encode();
            codec->decoder = rtp_ps_decode();
        }
		else if (0 == strcasecmp(encoding, "MP1S"))  // MPEG-1 Systems Streams video (RFC 2250)
		{
			codec->encoder = rtp_ts_encode();
			codec->decoder = rtp_ts_decode();
		}
		else if (0 == strcasecmp(encoding, "opus")	// RFC7587 RTP Payload Format for the Opus Speech and Audio Codec
			|| 0 == strcasecmp(encoding, "G726-16") // ITU-T G.726 audio 16 kbit/s (RFC 3551)
			|| 0 == strcasecmp(encoding, "G726-24")	// ITU-T G.726 audio 24 kbit/s (RFC 3551)
			|| 0 == strcasecmp(encoding, "G726-32") // ITU-T G.726 audio 32 kbit/s (RFC 3551)
			|| 0 == strcasecmp(encoding, "G726-40") // ITU-T G.726 audio 40 kbit/s (RFC 3551)
			|| 0 == strcasecmp(encoding, "G7221"))  // RFC5577 RTP Payload Format for ITU-T Recommendation G.722.1
		{
			codec->encoder = rtp_common_encode();
			codec->decoder = rtp_common_decode();
		}
		else
		{
			return -1;
		}
	}
	else
	{
#if defined(_DEBUG) || defined(DEBUG)
		const struct rtp_profile_t* profile;
		profile = rtp_profile_find(payload);
		assert(!profile || !encoding || !*encoding || 0 == strcasecmp(profile->name, encoding));
#endif

		switch (payload)
		{
		case RTP_PAYLOAD_PCMU: // ITU-T G.711 PCM u-Law audio 64 kbit/s (RFC 3551)
		case RTP_PAYLOAD_PCMA: // ITU-T G.711 PCM A-Law audio 64 kbit/s (RFC 3551)
		case RTP_PAYLOAD_G722: // ITU-T G.722 audio 64 kbit/s (RFC 3551)
		case RTP_PAYLOAD_G729: // ITU-T G.729 and G.729a audio 8 kbit/s (RFC 3551)
			codec->encoder = rtp_common_encode();
			codec->decoder = rtp_common_decode();
			break;

		case RTP_PAYLOAD_MP3: // MPEG-1 or MPEG-2 audio only (RFC 3551, RFC 2250)
		case RTP_PAYLOAD_MPV: // MPEG-1 and MPEG-2 video (RFC 2250)
			codec->encoder = rtp_mpeg1or2es_encode();
			codec->decoder = rtp_mpeg1or2es_decode();
			break;

		case RTP_PAYLOAD_MP2T: // MPEG-2 transport stream (RFC 2250)
			codec->encoder = rtp_ts_encode();
			codec->decoder = rtp_ts_decode();
			break;

		case RTP_PAYLOAD_JPEG:
		case RTP_PAYLOAD_H263:
			return -1; // TODO

		default:
			return -1; // not support
		}
	}

	return 0;
}
