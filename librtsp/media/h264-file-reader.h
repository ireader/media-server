#ifndef _h264_file_reader_h_
#define _h264_file_reader_h_

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <list>
#include "ctypedef.h"

class H264FileReader
{
public:
	H264FileReader(const char* file);
	~H264FileReader();

	bool IsOpened() const;

public:
    typedef std::vector<unsigned char> sps_t;
    const std::list<sps_t> GetParameterSets() const { return m_sps; }
	int GetDuration(int64_t& duration) const { duration = m_duration; return 0; }
	int GetNextFrame();
	int Seek(unsigned int pos);

private:
	int Init();
	const unsigned char* ReadNextFrame();

private:
	FILE* m_fp;
    typedef std::list<std::pair<size_t, long> > frames_t;
    frames_t m_videos;

    std::list<sps_t> m_sps;

    int64_t m_duration;
    unsigned char *m_ptr;
    size_t m_capacity, m_bytes, m_offset;
};

#endif /* !_h264_file_reader_h_ */
