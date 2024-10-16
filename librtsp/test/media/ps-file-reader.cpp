#include "ps-file-reader.h"
#include "mpeg-util.h"
#include "mpeg-types.h"
#include "mov-format.h"
#include "avcodecid.h"
#include "rtsp-payloads.h"
#include <inttypes.h>
#include <map>

PSFileReader::PSFileReader(const char* file)
	:m_fp(NULL), m_pos(0), m_v_start_ts(-1), m_v_end_ts(-1), m_v_codecid(-1), m_a_codecid(-1), m_duration(0), m_demuxer(NULL)
{
	memset(&m_utils, 0, sizeof(m_utils));
	m_fp = fopen(file, "rb");
	if (m_fp)
	{
		static struct ps_demuxer_notify_t notify = {
			PSOnStream,
		};
		m_demuxer = ps_demuxer_create(PSOnRead, this);
		ps_demuxer_set_notify(m_demuxer, &notify, this);

		m_pkts = std::shared_ptr<AVPacketQueue>(new AVPacketQueue(-1));

		Init();
	}
}

PSFileReader::~PSFileReader()
{
	avpktutil_destroy(&m_utils);

	if (m_demuxer)
	{
		ps_demuxer_destroy(m_demuxer);
		m_demuxer = NULL;
	}

	if (m_fp)
		fclose(m_fp);
}

int PSFileReader::Init()
{
	int n, i = 0, r = 0;
	while ((n = fread(m_packet + i, 1, sizeof(m_packet) - i, m_fp)) > 0)
	{
		r = ps_demuxer_input(m_demuxer, m_packet, n + i);
		assert(r == n + i);
		memmove(m_packet, m_packet + r, n + i - r);
		i = n + i - r;
	}
	while (i > 0 && r > 0)
	{
		r = ps_demuxer_input(m_demuxer, m_packet, i);
		memmove(m_packet, m_packet + r, i - r);
		i -= r;
	}

	if (m_v_start_ts >= 0 && m_v_end_ts >= 0)
	{
		m_duration = (m_v_end_ts - m_v_start_ts) / 90;
	}

	return 0;
}

int PSFileReader::Seek(int64_t& dts)
{
	int64_t fisrt_dts = -1;

	while (1)
	{
		std::shared_ptr<avpacket_t> pkt(m_pkts->Cur(), avpacket_release);
		if (NULL == pkt)
			return -1;

		if (fisrt_dts == -1)
			fisrt_dts = pkt->dts / 90;

		if (dts < fisrt_dts)
			break;

		if (dts >= (pkt->dts / 90))
		{
			// only audio
			if (m_v_start_ts < 0)
				return 0;

			if (pkt->flags & AVPACKET_FLAG_KEY)
				return 0;
		}
	}

	m_pkts->Reset();
	return 0;
}

int PSFileReader::OnPacket(struct avpacket_t* pkt)
{
	int ret = m_pkts->Push(pkt);
	m_pkts->Reset();

	return ret;
}

int PSFileReader::GetNextFrame(int64_t& pts, int64_t& dts, const uint8_t*& ptr, size_t& bytes, int& codecid, int& flags)
{
	if (m_pkts->End())
		return -1; // file end

	std::shared_ptr<avpacket_t> pkt(m_pkts->Cur(), avpacket_release);

	ptr = pkt->data;
	bytes = pkt->size;
	pts = pkt->pts;
	dts = pkt->dts;
	flags = pkt->flags;
	codecid = (pkt->stream->codecid >= AVCODEC_VIDEO_MPEG1 && pkt->stream->codecid <= AVCODEC_VIDEO_SVAC) ? m_v_codecid : m_a_codecid;

	return 0;
}

void PSFileReader::PSOnStream(void* param, int stream, int codecid, const void* extra, int bytes, int finish)
{
	printf("stream %d, codecid: %d, finish: %s\n", stream, codecid, finish ? "true" : "false");

	PSFileReader* self = (PSFileReader*)param;
	int r = avpayload_find_by_mpeg2(codecid);
	if (r == -1)
		return;

	AVPACKET_CODEC_ID avcodecid = s_payloads[r].codecid;
	if (avcodecid >= AVCODEC_VIDEO_MPEG1 && avcodecid <= AVCODEC_VIDEO_SVAC)
	{
		avpktutil_addvideo(&self->m_utils, stream, avcodecid, 0, 0, extra, bytes);
		self->m_v_codecid = codecid;
	}
	else if (avcodecid >= AVCODEC_AUDIO_PCM && avcodecid <= AVCODEC_AUDIO_SVAC)
	{
		avpktutil_addaudio(&self->m_utils, stream, avcodecid, 0, 0, 0, extra, bytes);
		self->m_a_codecid = codecid;
	}
}

inline const char* ftimestamp(int64_t t, char* buf)
{
	if (PTS_NO_VALUE == t)
	{
		sprintf(buf, "(null)");
	}
	else
	{
		t /= 90;
		sprintf(buf, "%d:%02d:%02d.%03d", (int)(t / 3600000), (int)((t / 60000) % 60), (int)((t / 1000) % 60), (int)(t % 1000));
	}
	return buf;
}

int PSFileReader::PSOnRead(void* param, int stream, int avtype, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	PSFileReader* self = (PSFileReader*)param;
	static std::map<int, std::pair<int64_t, int64_t>> s_streams;
	static char s_pts[64], s_dts[64];

	auto it = s_streams.find(stream);
	if (it == s_streams.end())
		it = s_streams.insert(std::make_pair(stream, std::pair<int64_t, int64_t>(pts, dts))).first;

	if (mpeg_stream_type_audio(avtype))
	{
		//assert(0 == a_dts || dts >= a_dts);
		printf("[A] pts: %s(%" PRId64 "), dts: %s(%" PRId64 "), diff: %03d/%03d, size: %u\n",
			ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - it->second.first) / 90,
			(int)(dts - it->second.second) / 90, (unsigned int)bytes);
	}
	else if (mpeg_stream_type_video(avtype))
	{
		//assert(0 == v_dts || dts >= v_dts);
		printf("[V] pts: %s(%" PRId64 "), dts: %s(%" PRId64 "), diff: %03d/%03d, size: %u%s\n",
			ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - it->second.first) / 90,
			(int)(dts - it->second.second) / 90, (unsigned int)bytes, (flags & MPEG_FLAG_IDR_FRAME) ? " [I]" : "");

		if (self->m_v_start_ts == -1)
			self->m_v_start_ts = dts < 0 ? pts : dts;
		self->m_v_end_ts = dts < 0 ? pts : dts;
	}
	else
	{
		//assert(0);
		//assert(0 == x_dts || dts >= x_dts);
		printf("[X] pts: %s(%" PRId64 "), dts: %s(%" PRId64 "), diff: %03d/%03d\n",
			ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - it->second.first), (int)(dts - it->second.second));
	}

	it->second = std::make_pair(pts, dts);

	for (int i = 0; i < self->m_utils.count; i++)
	{
		if (self->m_utils.streams[i]->stream == stream)
		{
			struct avpacket_t* pkt = NULL;
			avpktutil_input(&self->m_utils, self->m_utils.streams[i], data, bytes, pts, dts, flags, &pkt);
			self->OnPacket(pkt);
			return 0;
		}
	}

	return -1;
}
