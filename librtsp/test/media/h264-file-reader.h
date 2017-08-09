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
    const std::list<std::pair<const uint8_t*, size_t> > GetParameterSets() const { return m_sps; }
	int GetDuration(int64_t& duration) const { duration = m_duration; return 0; }
    int GetNextFrame(int64_t &dts, const uint8_t* &ptr, size_t &bytes);
	int Seek(int64_t &dts);

private:
	int Init();

private:
	struct vframe_t
	{
		const uint8_t* nalu;
		int64_t time;
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

    std::list<std::pair<const uint8_t*, size_t> > m_sps;
	int64_t m_duration;

    uint8_t *m_ptr;
    size_t m_capacity;
};

#endif /* !_h264_file_reader_h_ */
