#include "h265-file-reader.h"
#include <assert.h>
#include <string.h>
#include <algorithm>

#define H265_NAL(v)	 ((v>> 1) & 0x3f)

enum { NAL_IDR_W_RADL = 19, NAL_IDR_N_LP= 20, NAL_VPS = 32, NAL_SPS = 33, NAL_PPS = 34, NAL_SEI = 39};

H265FileReader::H265FileReader(const char* file)
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

H265FileReader::~H265FileReader()
{    
	if (m_ptr)
	{
		assert(m_capacity > 0);
		free(m_ptr);
	}
}

bool H265FileReader::IsOpened() const
{
	return !m_videos.empty();
}

int H265FileReader::GetNextFrame(int64_t &dts, const uint8_t* &ptr, size_t &bytes)
{
	if(m_vit == m_videos.end())
		return -1; // file end

	ptr = m_vit->nalu;
	dts = m_vit->time;
	bytes = m_vit->bytes;

	++m_vit;
	return 0;
}

int H265FileReader::Seek(int64_t &dts)
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

static inline int h265_nal_type(const unsigned char* ptr)
{
    int i = 2;
    assert(0x00 == ptr[0] && 0x00 == ptr[1]);
    if(0x00 == ptr[2])
        ++i;
    assert(0x01 == ptr[i]);
    return H265_NAL(ptr[i+1]);
}


int H265FileReader::Init()
{
    size_t count = 0;
    bool vpsspspps = true;

	const uint8_t* end = m_ptr + m_capacity;
    const uint8_t* nalu = search_start_code(m_ptr, end);
	const uint8_t* p = nalu;

	while (p < end)
	{
        const unsigned char* pn = search_start_code(p + 4, end);
		size_t bytes = pn - nalu;

        int nal_unit_type = h265_nal_type(p);
		assert(0 <= nal_unit_type);

        if(NAL_VPS == nal_unit_type || NAL_SPS == nal_unit_type || NAL_PPS == nal_unit_type)
        {
            if(vpsspspps)
            {
                size_t n = 0x01 == p[2] ? 3 : 4;
				std::pair<const uint8_t*, size_t> pr;
				pr.first = p + n;
				pr.second = bytes;
				m_sps.push_back(pr);
            }
        }
		
        {
            if(m_sps.size() > 0) vpsspspps = false; // don't need more vps/sps/pps

			vframe_t frame;
			frame.nalu = nalu;
			frame.bytes = bytes;
			frame.time = 40 * count++;
			frame.idr = (NAL_IDR_N_LP == nal_unit_type || NAL_IDR_W_RADL == nal_unit_type); // IDR-frame
			m_videos.push_back(frame);
			nalu = pn;
        }

        p = pn;
    }

    m_duration = 40 * count;
    return 0;
}
