#include "ctypedef.h"
#include "rtp-pack.h"

// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams
// 6. RTP Packetization of MPEG-4 Audio Bitstreams (p15)
// 7.3 Media Type Registration for MPEG-4 Audio (p21)
struct rtp_pack_t *rtp_mp4v_es_packer()
{
	return rtp_ps_packer();
}
