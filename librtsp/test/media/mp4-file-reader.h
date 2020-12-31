#ifndef _mp4_file_reader_h_
#define _mp4_file_reader_h_

#include "vod-file-source.h"
#include "mov-reader.h"
#include <string>
#include <stdio.h>
#include <stdint.h>

class MP4FileReader : public VodFileSource::IFileReader
{
public:
	MP4FileReader(const char* file);
	virtual ~MP4FileReader();

public:
	virtual int Read(struct avpacket_t** pkt);
	virtual int Seek(uint64_t* pos, int strategy);
	virtual uint64_t GetPosotion();
	virtual uint64_t GetDuration();

public:
	int GetInfo(struct mov_reader_trackinfo_t *ontrack, void* param);

private:
	static void MP4OnRead(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags);

private:
	FILE* m_fp;
	int64_t m_pos;
	mov_reader_t* m_reader;
	uint8_t m_packet[2 * 1024 * 1024];
};

#endif /* !_mp4_file_reader_h_ */
