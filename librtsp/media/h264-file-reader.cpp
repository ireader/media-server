#include "h264-file-reader.h"
#include <assert.h>

#define H264_NAL(v)	(v & 0x1F)

enum { NAL_IDR = 5, NAL_SEI = 6, NAL_SPS = 7, NAL_PPS = 8 };

H264FileReader::H264FileReader(const char* file)
{
	m_fp = fopen(file, "rb");
    if(m_fp)
    {
        m_offset = 0;
        m_capacity = 128 * 1024;
        m_ptr = (unsigned char*)realloc(m_ptr, m_capacity);
        m_bytes = fread(m_ptr, 1, m_capacity, m_fp);
        if(m_bytes > 0)
            Init();
    }
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

inline const unsigned char* search_start_code(const unsigned char* ptr, size_t bytes)
{
    const unsigned char *p;
    for(p = ptr; p + 3 < ptr + bytes; p++)
    {
        if(0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00==p[2] && 0x01==p[3])))
            return p;
    }
    return NULL;
}

inline int h264_nal_type(const unsigned char* ptr)
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
                m_ptr = (unsigned char*)realloc(m_ptr, m_capacity + m_capacity/2);
                if(m_ptr)
                {
                    m_capacity += m_capacity/2;

                    // 2. read file
                    assert(0 == m_offset);
                    n = fread(m_ptr + m_bytes, 1, m_capacity - m_bytes, m_fp);
                    m_bytes += n;
                }
            }
        }
    } while(!p && m_ptr && m_capacity < 10*1024*1024 && n > 0); // Max frame size 10MB
    
    return p;
}

int H264FileReader::Init()
{
	assert(IsOpened());
	assert(0 == ftell(m_fp));
    assert(0 == search_start_code(m_ptr, m_bytes));

    size_t count = 0;
    bool spspps = true;
    const unsigned char* nalu = m_ptr;

	do
	{
        const unsigned char* nalu2 = ReadNextFrame();

        int nal_unit_type = h264_nal_type(nalu);
        assert(0 != nal_unit_type);
        if(nal_unit_type <= 5)
        {
            if(m_sps.size() > 0) spspps = false; // don't need more sps/pps

            // IDR-frame
            if(5 == nal_unit_type)
            {
                long n = ftell(m_fp);
                long pos = n - (m_bytes - (nalu - m_ptr));
                m_videos.push_back(std::make_pair(40*count, pos));
            }
            
            ++count;
        }
        else if(NAL_SPS == nal_unit_type || NAL_PPS == nal_unit_type)
        {
            assert(nalu2);
            if(spspps && nalu2)
            {
                size_t n = 0x01 == nalu[2] ? 3 : 4;
                sps_t sps(nalu2 - nalu - n);
                memcpy(&sps[0], nalu, nalu2-nalu-n);
            }
        }

        nalu = nalu2;
        m_offset = nalu - m_ptr;
    } while(nalu);
    
    m_duration = 40 * count;
    return 0;
}
