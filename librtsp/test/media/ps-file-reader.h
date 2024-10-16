#ifndef _ps_file_reader_h_
#define _ps_file_reader_h_

#include "vod-file-source.h"
#include "mpeg-ps.h"
#include "avpktutil.h"
#include <string>
#include <stdio.h>
#include <stdint.h>

class PSFileReader : std::enable_shared_from_this<PSFileReader>
{
public:
	PSFileReader(const char* file);
	virtual ~PSFileReader();

public:
	virtual int OnPacket(struct avpacket_t* pkt);

	int GetDuration(int64_t& duration) const { duration = m_duration; return 0; }
	int GetNextFrame(int64_t& pts, int64_t& dts, const uint8_t*& ptr, size_t& bytes, int& codecid, int& flags);
	int Seek(int64_t& dts);

private:
	int Init();

	static void PSOnStream(void* param, int stream, int codecid, const void* extra, int bytes, int finish);
	static int PSOnRead(void* param, int stream, int avtype, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes);

public:
	int64_t m_v_start_ts;
	int64_t m_v_end_ts;
	int m_v_codecid;
	int m_a_codecid;

private:
	FILE* m_fp;
	int64_t m_pos;
	int64_t m_duration;
	ps_demuxer_t* m_demuxer;
	struct avpacket_t** m_pkt;
	struct avpktutil_t m_utils;
	uint8_t m_packet[2 * 1024 * 1024];

	std::shared_ptr<AVPacketQueue> m_pkts;
};

#endif /* !_ps_file_reader_h_ */
