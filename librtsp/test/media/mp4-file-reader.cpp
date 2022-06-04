#include "mp4-file-reader.h"
#include "mov-format.h"
#include "avcodecid.h"
#include "rtsp-payloads.h"

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

MP4FileReader::MP4FileReader(const char* file)
	:m_fp(NULL), m_pos(0), m_reader(NULL)
{
	memset(&m_utils, 0, sizeof(m_utils));
	m_fp = fopen(file, "rb");
	if (m_fp)
	{
		m_reader = mov_reader_create(mov_file_buffer(), m_fp);
		GetInfo();
	}
}

MP4FileReader::~MP4FileReader()
{
	avpktutil_destroy(&m_utils);

	if (m_reader)
	{
		mov_reader_destroy(m_reader);
		m_reader = NULL;
	}

	if (m_fp)
		fclose(m_fp);
}

int MP4FileReader::GetInfo()
{
	if (!m_reader)
		return -1;
	struct mov_reader_trackinfo_t info = { OnVideoInfo, OnAudioInfo, OnSubtitleInfo};
	return mov_reader_getinfo(m_reader, &info, this);
}

int MP4FileReader::Read(struct avpacket_t** pkt)
{
	m_pkt = pkt;
	int r = mov_reader_read(m_reader, m_packet, sizeof(m_packet), MP4OnRead, this);
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

void MP4FileReader::OnVideoInfo(void* param, uint32_t track, uint8_t object, int width, int height, const void* extra, size_t bytes)
{
	MP4FileReader* self = (MP4FileReader*)param;
	int r = avpayload_find_by_mov(object);
	if (r == -1)
		return;
	avpktutil_addvideo(&self->m_utils, track, s_payloads[r].codecid, width, height, extra, bytes);
}

void MP4FileReader::OnAudioInfo(void* param, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes)
{
	MP4FileReader* self = (MP4FileReader*)param;
	int r = avpayload_find_by_mov(object);
	if (r == -1)
		return;
	avpktutil_addaudio(&self->m_utils, track, s_payloads[r].codecid, channel_count, bit_per_sample, sample_rate, extra, bytes);
}

void MP4FileReader::OnSubtitleInfo(void* param, uint32_t track, uint8_t object, const void* extra, size_t bytes)
{
	MP4FileReader* self = (MP4FileReader*)param;
	int r = avpayload_find_by_mov(object);
	if (r == -1)
		return;
	avpktutil_addsubtitle(&self->m_utils, track, s_payloads[r].codecid, extra, bytes);
}

void MP4FileReader::MP4OnRead(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	MP4FileReader* self = (MP4FileReader*)param;
	avpktutil_input(&self->m_utils, self->m_utils.streams[track-1], buffer, bytes, pts, dts, flags, self->m_pkt);
}
