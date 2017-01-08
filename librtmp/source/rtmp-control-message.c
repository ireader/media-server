#include "rtmp-control-message.h"
#include "byte-order.h"

// 5.4.1. Set Chunk Size (1) (p19)
int rtmp_set_chunk_size(uint8_t* out, size_t size, uint32_t chunkSize)
{
	if (size < 4) return 0;
	be_write_uint32(out, chunkSize);
	out[0] &= 0x7F; // first bit MUST be zero
	return 4;
}

// 5.4.2. Abort Message (2)  (p19)
int rtmp_abort_message(uint8_t* out, size_t size, uint32_t chunkStreamId)
{
	if (size < 4) return 0;
	be_write_uint32(out, chunkStreamId);
	return 4;
}

// 5.4.3. Acknowledgement (3) (p20)
int rtmp_acknowledgement(uint8_t* out, size_t size, uint32_t sequenceNumber)
{
	if (size < 4) return 0;
	be_write_uint32(out, sequenceNumber);
	return 4;
}

// 5.4.4. Window Acknowledgement Size (5) (p20)
int rtmp_window_acknowledgement_size(uint8_t* out, size_t size, uint32_t windowSize)
{
	if (size < 4) return 0;
	be_write_uint32(out, windowSize);
	return 4;
}

// 5.4.5. Set Peer Bandwidth (6) (p21)
int rtmp_set_peer_bandwidth(uint8_t* out, size_t size, uint32_t windowSize, uint8_t limitType)
{
	if (size < 5) return 0;
	be_write_uint32(out, windowSize);
	out[4] = limitType;
	return 5;
}
