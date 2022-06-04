#ifndef _mp4_file_reader_h_
#define _mp4_file_reader_h_

#include "vod-file-source.h"
#include "mov-reader.h"
#include "avpktutil.h"
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
	int GetInfo();

private:
	static void OnVideoInfo(void* param, uint32_t track, uint8_t object, int width, int height, const void* extra, size_t bytes);
	static void OnAudioInfo(void* param, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes);
	static void OnSubtitleInfo(void* param, uint32_t track, uint8_t object, const void* extra, size_t bytes);
	static void MP4OnRead(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags);

private:
	FILE* m_fp;
	int64_t m_pos;
	mov_reader_t* m_reader;
	struct avpacket_t** m_pkt;
	struct avpktutil_t m_utils;
	uint8_t m_packet[2 * 1024 * 1024];
};

#endif /* !_mp4_file_reader_h_ */
