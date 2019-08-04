#include "mp4-file-reader.h"
#include "mov-format.h"

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

MP4FileReader::MP4FileReader(const char* file)
	:m_fp(NULL), m_pos(0), m_reader(NULL)
{
	m_fp = fopen(file, "rb");
	if (m_fp)
	{
		m_reader = mov_reader_create(mov_file_buffer(), m_fp);
	}
}

MP4FileReader::~MP4FileReader()
{
	if (m_reader)
	{
		mov_reader_destroy(m_reader);
		m_reader = NULL;
	}

	if (m_fp)
		fclose(m_fp);
}

int MP4FileReader::GetInfo(struct mov_reader_trackinfo_t *ontrack, void* param)
{
	return mov_reader_getinfo(m_reader, ontrack, param);
}

int MP4FileReader::Read(struct avpacket_t** pkt)
{
	int r = mov_reader_read(m_reader, m_packet, sizeof(m_packet), MP4OnRead, pkt);
	if (r < 0 || NULL == *pkt)
		return *pkt ? r : -1; //ENOMEM

	if(*pkt)
		m_pos = (*pkt)->dts; // update offset
	return r;
}

int MP4FileReader::Seek(uint64_t* pos, int strategy)
{
	int r = mov_reader_seek(m_reader, (int64_t*)pos);
	if (0 == r)
		m_pos = (int64_t)*pos;
	return r;
}

uint64_t MP4FileReader::GetPosotion()
{
	return m_pos;
}

uint64_t MP4FileReader::GetDuration()
{
	return mov_reader_getduration(m_reader);
}

void MP4FileReader::MP4OnRead(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	struct avpacket_t* pkt;
	struct avpacket_t** pp = (struct avpacket_t**)param;

	pkt = avpacket_alloc(bytes);
	if (pkt)
	{
		memcpy(pkt->data, buffer, bytes);
		pkt->stream = track;
		pkt->pts = pts;
		pkt->dts = dts;
		pkt->flags = flags ? AVPACKET_FLAG_KEY : 0;
		*pp = pkt;
	}
}
