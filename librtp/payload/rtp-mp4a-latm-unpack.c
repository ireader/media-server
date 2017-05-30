#include "ctypedef.h"
#include "rtp-unpack.h"

// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams
struct rtp_unpack_t *rtp_mp4a_latm_unpacker()
{
	return rtp_ps_unpacker();
}
