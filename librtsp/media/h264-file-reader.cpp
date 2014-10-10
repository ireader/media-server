#include "h264-file-reader.h"

#define BUFFER_SIZE (8*2014)
#define H264_NAL(v)	(v & 0x1F)

enum { NAL_IDR = 5, NAL_SEI = 6, NAL_SPS = 7, NAL_PPS = 8 };

H264FileReader::H264FileReader(const char* file)
{
	m_fp = fopen(file, "rb");
	m_ptr.reserve(BUFFER_SIZE);
	Init();
}

H264FileReader::~H264FileReader()
{
	if(m_fp)
	{
		fclose(m_fp);
		m_fp = NULL;
	}
}

bool H264FileReader::IsOpened() const
{
	return !!m_fp;
}

int H264FileReader::Init()
{
	assert(IsOpened());
	assert(0 == ftell(m_fp));

	int bytes1, bytes2;
	unsigned char *data1, *data2;

	int r = 0;

	do
	{
		do
		{
			r = GetNextNALU(data1, bytes1);
		} while(0 == r && NAL_SPS != H264_NAL(data1[0]));
		
		if(0 != r)
			return -1;

		r = GetNextNALU(data2, bytes2);
	} while(0 == r && NAL_PPS != H264_NAL(data2[0]));

	if(0 != r)
		return -1;

	m_spspps.append(data1, bytes1);
	m_spspps.append(data2, bytes2);
	return 0;
}

static const unsigned char* search_start_code(const unsigned char* stream, int bytes)
{
	const unsigned char *p;
	for(p = stream; p+3<stream+bytes; p++)
	{
		if(0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00==p[2] && 0x01==p[3])))
			return p;
	}
	return NULL;
}

// don't include startcode(0x00000001)
int H264FileReader::GetNextNALU(unsigned char*& data, int& bytes)
{
	assert(m_ptr.capacity() >= BUFFER_SIZE);
	if(m_ptr.size() < 4)
	{
		int n = fread((char*)m_ptr.get() + m_ptr.size(), 1, m_ptr.capacity() - m_ptr.size(), m_fp);
		if(0 == n)
			return -1;
	}

	//const unsigned char* p = NULL;
	//p = (const unsigned char*)m_ptr.get();
	//p = search_start_code(p, m_ptr.size());
	//if(p)
	//{
	//	offset = p - (const unsigned char*)m_ptr.get();
	//	break;
	//}

	//const unsigned char* pn = search_start_code(p, m_ptr.capacity()-4);
	//while(!pn)
	//{
	//	int capacity = m_ptr.capacity();
	//	m_ptr.reserve(m_ptr.capacity() + BUFFER_SIZE);
	//	p = (const unsigned char*)m_ptr.get() + capacity;

	//	if(0 == fread(p, 1, m_ptr.capacity() - capacity, m_fp))
	//		return -1;

	//	pn = search_start_code(p-4, m_ptr.capacity() - capacity  + 4);
	//}
	//while(p)
	//{
	//	int prefix = 0x01000000 == *(int*)startcode ? 4 : 3;
	//	unsigned int nal_unit_type = 0x1F & startcode[prefix];
	//	if( (nal_unit_type>0 && nal_unit_type <= 5) // data slice
	//		|| 10==nal_unit_type // end of sequence
	//		|| 11==nal_unit_type) // end of stream
	//	{
	//		Decode(pframe, p-pframe);
	//		pframe = p;
	//	}

	//	startcode = p;
	//	p = search_start_code(startcode+4, pend-startcode-4);
	//}
}

const mmptr& H264FileReader::GetSPSandPPS() const
{
	return m_ptr;
}
