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
    int GetNextFrame(int64_t &pos, void* &ptr, size_t &bytes);
	int Seek(int64_t &pos);

private:
	int Init();
	const unsigned char* ReadNextFrame();

private:
	FILE* m_fp;
	struct vframe_t
	{
		int64_t time;
		long offset;
		long bytes;
		bool idr; // IDR frame

		bool operator < (const struct vframe_t &v) const
		{
			return time < v.time;
		}
	};
    typedef std::vector<vframe_t> vframes_t;
    vframes_t m_videos;
	vframes_t::iterator m_vit;

    std::list<sps_t> m_sps;

    int64_t m_duration;
    unsigned char *m_ptr;
    size_t m_capacity, m_bytes, m_offset;
};

#endif /* !_h264_file_reader_h_ */
