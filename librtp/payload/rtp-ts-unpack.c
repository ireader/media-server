#include "ctypedef.h"
#include "rtp-unpack.h"

// same as ps unpacker
struct rtp_unpack_t *rtp_ts_unpacker()
{
	return rtp_ps_unpacker();
}
