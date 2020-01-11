#include "h264-file-reader.h"
#include <assert.h>
#include <string.h>
#include <algorithm>

#define H264_NAL(v)	(v & 0x1F)

enum { NAL_IDR = 5, NAL_SEI = 6, NAL_SPS = 7, NAL_PPS = 8 };

H264FileReader::H264FileReader(const char* file)
:m_ptr(NULL), m_capacity(0)
{
	FILE* fp = fopen(file, "rb");
    if(fp)
    {
		fseek(fp, 0, SEEK_END);
		m_capacity = ftell(fp);
		fseek(fp, 0, SEEK_SET);

        m_ptr = (uint8_t*)malloc(m_capacity);
		fread(m_ptr, 1, m_capacity, fp);
		fclose(fp);

		Init();
    }

	m_vit = m_videos.begin();
}

H264FileReader::~H264FileReader()
{    
	if (m_ptr)
	{
		assert(m_capacity > 0);
		free(m_ptr);
	}
}

bool H264FileReader::IsOpened() const
{
	return !m_videos.empty();
}

int H264FileReader::GetNextFrame(int64_t &dts, const uint8_t* &ptr, size_t &bytes)
{
	if(m_vit == m_videos.end())
		return -1; // file end

	ptr = m_vit->nalu;
	dts = m_vit->time;
	bytes = m_vit->bytes;

	++m_vit;
	return 0;
}

int H264FileReader::Seek(int64_t &dts)
{
	vframe_t frame;
	frame.time = dts;

	vframes_t::iterator it;
	it = std::lower_bound(m_videos.begin(), m_videos.end(), frame);
	if(it == m_videos.end())
		return -1;

	while(it != m_videos.begin())
	{
		if(it->idr)
		{
			m_vit = it;
			return 0;
		}
		--it;
	}
	return 0;
}

static inline const uint8_t* search_start_code(const uint8_t* ptr, const uint8_t* end)
{
    for(const uint8_t *p = ptr; p + 3 < end; p++)
    {
        if(0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00==p[2] && 0x01==p[3])))
            return p;
    }
	return end;
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

static inline int h264_nal_new_access(const unsigned char* ptr, const uint8_t* end)
{
	int i = 2;
	if (end - ptr < 4)
		return 1;
	assert(0x00 == ptr[0] && 0x00 == ptr[1]);
	if (0x00 == ptr[2])
		++i;
	assert(0x01 == ptr[i]);
	int nal_unit_type = H264_NAL(ptr[i + 1]);
	if (nal_unit_type < 1 || nal_unit_type > 5)
		return 1;

	if (ptr + i + 2 > end)
		return 1;

	// Live555 H264or5VideoStreamParser::parse
	// The high-order bit of the byte after the "nal_unit_header" tells us whether it's
	// the start of a new 'access unit' (and thus the current NAL unit ends an 'access unit'):
	return (ptr[i + 2] & 0x80) != 0 ? 1 : 0;
}

int H264FileReader::Init()
{
    size_t count = 0;
    bool spspps = true;

	const uint8_t* end = m_ptr + m_capacity;
    const uint8_t* nalu = search_start_code(m_ptr, end);
	const uint8_t* p = nalu;

	while (p < end)
	{
        const unsigned char* pn = search_start_code(p + 4, end);
		size_t bytes = pn - nalu;

        int nal_unit_type = h264_nal_type(p);
		assert(0 != nal_unit_type);
        if(nal_unit_type <= 5 && h264_nal_new_access(pn, end))
        {
            if(m_sps.size() > 0) spspps = false; // don't need more sps/pps

			vframe_t frame;
			frame.nalu = nalu;
			frame.bytes = bytes;
			frame.time = 40 * count++;
			frame.idr = 5 == nal_unit_type; // IDR-frame
			m_videos.push_back(frame);
			nalu = pn;
        }
        else if(NAL_SPS == nal_unit_type || NAL_PPS == nal_unit_type)
        {
            if(spspps)
            {
                size_t n = 0x01 == p[2] ? 3 : 4;
				std::pair<const uint8_t*, size_t> pr;
				pr.first = p + n;
				pr.second = bytes;
				m_sps.push_back(pr);
            }
        }

        p = pn;
    }

    m_duration = 40 * count;
    return 0;
}
