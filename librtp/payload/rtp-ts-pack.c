#include "ctypedef.h"
#include "rtp-pack.h"
#include "rtp-profile.h"
#include <stdlib.h>
#include <assert.h>

#define TS_PACKET_SIZE 188

static void* rtp_ts_pack_create(uint8_t pt, uint16_t seq, uint32_t ssrc, uint32_t frequency, struct rtp_pack_func_t *func, void* param)
{
	assert(RTP_PAYLOAD_MP2T == pt);
	return rtp_ps_packer()->create(pt, seq, ssrc, frequency, func, param);
}

static void rtp_ts_pack_destroy(void* pack)
{
	rtp_ps_packer()->destroy(pack);
}

static int rtp_ts_pack_input(void* pack, const void* ts, size_t bytes, int64_t time)
{
	assert(0 == bytes % TS_PACKET_SIZE);
	rtp_pack_setsize(rtp_pack_getsize() / TS_PACKET_SIZE * TS_PACKET_SIZE); // must be x188
	return rtp_ps_packer()->input(pack, ts, bytes, time);
}

static void rtp_ts_pack_get_info(void* pack, unsigned short* seq, unsigned int* timestamp)
{
	rtp_ps_packer()->get_info(pack, seq, timestamp);
}

struct rtp_pack_t *rtp_ts_packer()
{
	static struct rtp_pack_t packer = {
		rtp_ts_pack_create,
		rtp_ts_pack_destroy,
		rtp_ts_pack_get_info,
		rtp_ts_pack_input,
	};

	return &packer;
}
