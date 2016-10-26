#include "h264-file-reader.h"
#include <assert.h>
#include <memory.h>
#include <algorithm>

#define H264_NAL(v)	(v & 0x1F)

enum { NAL_IDR = 5, NAL_SEI = 6, NAL_SPS = 7, NAL_PPS = 8 };

H264FileReader::H264FileReader(const char* file)
:m_ptr(NULL), m_capacity(0), m_bytes(0)
{
	m_fp = fopen(file, "rb");
    if(m_fp)
    {
        m_offset = 0;
        m_capacity = 128 * 1024;
        m_ptr = (unsigned char*)malloc(m_capacity);
		if(m_ptr)
		{
			m_bytes = fread(m_ptr, 1, m_capacity, m_fp);
			if(m_bytes > 0)
				Init();

			fseek(m_fp, 0, SEEK_SET);
		}
    }

	m_vit = m_videos.begin();
}

H264FileReader::~H264FileReader()
{
	if(m_fp)
	{
		fclose(m_fp);
		m_fp = NULL;
	}
    
    if(m_ptr)
    {
        assert(m_capacity > 0);
        free(m_ptr);
    }
}

bool H264FileReader::IsOpened() const
{
	return !!m_fp;
}

int H264FileReader::GetNextFrame(int64_t &pos, void* &ptr, size_t &bytes)
{
	if(m_vit == m_videos.end())
		return -1; // file end

	size_t n = fread(m_ptr, 1, m_vit->bytes, m_fp);
	assert((long)n == m_vit->bytes);

	ptr = m_ptr;
	pos = m_vit->time;
	bytes = m_vit->bytes;

	++m_vit;
	return 0;
}

int H264FileReader::Seek(int64_t &pos)
{
	vframe_t frame;
	frame.time = pos;

	vframes_t::iterator it;
	it = std::lower_bound(m_videos.begin(), m_videos.end(), frame);
	if(it == m_videos.end())
		return -1;

	while(it != m_videos.begin())
	{
		if(it->idr)
		{
			fseek(m_fp, it->offset, SEEK_SET);
			pos = it->time;
			m_vit = it;
			return 0;
		}
		--it;
	}
	return 0;
}

static inline const unsigned char* search_start_code(const unsigned char* ptr, size_t bytes)
{
    const unsigned char *p;
    for(p = ptr; p + 3 < ptr + bytes; p++)
    {
        if(0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00==p[2] && 0x01==p[3])))
            return p;
    }
    return NULL;
}

static inline int h264_nal_type(const unsigned char* ptr)
{
    int i = 2;
    assert(0x00 == ptr[0] && 0x00 == ptr[1]);
    if(0x00 == ptr[2])
        ++i;
    assert(0x01 == ptr[i]);
    return H264_NAL(ptr[i+1]);
}

const unsigned char* H264FileReader::ReadNextFrame()
{
    size_t n = 0;
    const unsigned char* p;
    assert(m_ptr+m_offset == search_start_code(m_ptr+m_offset, m_bytes-m_offset));
    
    do
    {
        p = search_start_code(m_ptr + m_offset + 3, m_bytes - m_offset - 3);
        if(!p)
        {
            if(m_offset > 0)
            {
                memmove(m_ptr, m_ptr+m_offset, m_bytes-m_offset);
                m_bytes -= m_offset;
                m_offset = 0;

                // try read file
                assert(m_bytes < m_capacity);
                n = fread(m_ptr + m_bytes, 1, m_capacity - m_bytes, m_fp);
                m_bytes += n;
            }
            else
            {
                // 1. more memory
				unsigned char* ptr = NULL;
                ptr = (unsigned char*)realloc(m_ptr, m_capacity + m_capacity/2);
                if(ptr)
                {
					m_ptr = ptr;
                    m_capacity += m_capacity/2;

                    // 2. read file
                    assert(0 == m_offset);
                    n = fread(m_ptr + m_bytes, 1, m_capacity - m_bytes, m_fp);
                    m_bytes += n;
                }
				else
				{
					break; // don't have enough memory
				}
            }
        }
    } while(!p && m_ptr && m_capacity < 10*1024*1024 && n > 0); // Max frame size 10MB
    
    return p;
}

#define toOffset(ptr) (n - (m_bytes - (ptr - m_ptr)))

int H264FileReader::Init()
{
	//assert(IsOpened());
	//assert(0 == ftell(m_fp));
    assert(m_ptr == search_start_code(m_ptr, m_bytes));

	long offset = 0;
    size_t count = 0;
    bool spspps = true;
    const unsigned char* nalu = m_ptr;

	do
	{
        const unsigned char* nalu2 = ReadNextFrame();

		nalu = m_ptr + m_offset;
        int nal_unit_type = h264_nal_type(nalu);
        assert(0 != nal_unit_type);
        if(nal_unit_type <= 5)
        {
            if(m_sps.size() > 0) spspps = false; // don't need more sps/pps

			long n = ftell(m_fp);

			vframe_t frame;
			frame.offset = offset;
			frame.bytes = (nalu2 ? toOffset(nalu2) : n) - offset;
			frame.time = 40 * count++;
			frame.idr = 5 == nal_unit_type; // IDR-frame
			m_videos.push_back(frame);
			offset += frame.bytes;
        }
        else if(NAL_SPS == nal_unit_type || NAL_PPS == nal_unit_type)
        {
            assert(nalu2);
            if(spspps && nalu2)
            {
                size_t n = 0x01 == nalu[2] ? 3 : 4;
                sps_t sps(nalu2 - nalu - n);
                memcpy(&sps[0], nalu+n, nalu2-nalu-n);

				// filter last 0x00 bytes
				while(sps.size() > 0 && !*sps.rbegin())
					sps.resize(sps.size()-1);
				m_sps.push_back(sps);
            }
        }

        nalu = nalu2;
        m_offset = nalu - m_ptr;
    } while(nalu);

    m_duration = 40 * count;
    return 0;
}
