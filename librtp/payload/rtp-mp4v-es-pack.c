#include "ctypedef.h"
#include "rtp-pack.h"

// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams
// 5. RTP Packetization of MPEG-4 Visual Bitstreams (p8)
// 7.1 Media Type Registration for MPEG-4 Audio/Visual Streams (p17)
struct rtp_pack_t *rtp_mp4v_es_packer()
{
	return rtp_ps_packer();
}
