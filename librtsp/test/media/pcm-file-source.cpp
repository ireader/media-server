#include "pcm-file-source.h"
#include "cstringext.h"
#include "base64.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include <assert.h>

extern "C" uint32_t rtp_ssrc(void);

PCMFileSource::PCMFileSource(const char *file)
{
	m_fp = fopen(file, "rb");

	m_speed = 1.0;
	m_status = 0;
	m_rtp_clock = 0;
	m_rtcp_clock = 0;

	uint32_t ssrc = rtp_ssrc();
	static struct rtp_payload_t s_rtpfunc = {
		PCMFileSource::RTPAlloc,
		PCMFileSource::RTPFree,
		PCMFileSource::RTPPacket,
	};
	m_rtppacker = rtp_payload_encode_create(RTP_PAYLOAD_PCMA, "PCMA", (uint16_t)ssrc, ssrc, &s_rtpfunc, this);

	struct rtp_event_t event;
	event.on_rtcp = OnRTCPEvent;
	m_rtp = rtp_create(&event, this, ssrc, ssrc, 8000, 8000, 1);
	rtp_set_info(m_rtp, "RTSPServer", "test.pcm");
}

PCMFileSource::~PCMFileSource()
{
	if (m_fp)
		fclose(m_fp);
	m_fp = NULL;

	if (m_rtp)
		rtp_destroy(m_rtp);

	if (m_rtppacker)
		rtp_payload_encode_destroy(m_rtppacker);
}

int PCMFileSource::SetTransport(const char* /*track*/, std::shared_ptr<IRTPTransport> transport)
{
	m_transport = transport;
	return 0;
}

int PCMFileSource::Play()
{
	if (!m_fp)
		return -1;
	m_status = 1;

	static uint32_t timestamp = 0;
	time64_t clock = time64_now();
	if (0 == m_rtp_clock)
		m_rtp_clock = clock;

	if (m_rtp_clock + 20 < clock)
	{
		uint8_t ptr[20 * 8]; // 20ms
		if (sizeof(ptr) == fread(ptr, 1, sizeof(ptr), m_fp))
		{
			rtp_payload_encode_input(m_rtppacker, ptr, sizeof(ptr), timestamp * 8 /*kHz*/);
			m_rtp_clock += 20;
			timestamp += 20;

			SendRTCP();
			return 1;
		}
	}

	return 0;
}

int PCMFileSource::Pause()
{
	m_status = 2;
	m_rtp_clock = 0;
	return 0;
}

int PCMFileSource::Seek(int64_t pos)
{
	if (!m_fp) return -1;
	m_pos = pos;
	m_rtp_clock = 0;
	return fseek(m_fp, pos * 8, SEEK_CUR);
}

int PCMFileSource::SetSpeed(double speed)
{
	m_speed = speed;
	return 0;
}

int PCMFileSource::GetDuration(int64_t& duration) const
{
	if (!m_fp) return 0;
	long off = ftell(m_fp);
	fseek(m_fp, 0, SEEK_END);
	long total = ftell(m_fp);
	duration = total / 8; // 8000kHz, 8bits
	fseek(m_fp, off, SEEK_SET);
	return 0;
}

int PCMFileSource::GetSDPMedia(std::string& sdp) const
{
	static const char* pattern =
		"m=audio 0 RTP/AVP %d\n";

	char m[64];
	snprintf(m, sizeof(m), pattern, RTP_PAYLOAD_PCMA);
	sdp = m;
	return 0;
}

int PCMFileSource::GetRTPInfo(const char* uri, char *rtpinfo, size_t bytes) const
{
	uint16_t seq;
	uint32_t timestamp;
	rtp_payload_encode_getinfo(m_rtppacker, &seq, &timestamp);

	// url=rtsp://video.example.com/twister/video;seq=12312232;rtptime=78712811
	snprintf(rtpinfo, bytes, "url=%s;seq=%hu;rtptime=%u", uri, seq, timestamp);
	return 0;
}

void PCMFileSource::OnRTCPEvent(const struct rtcp_msg_t* msg)
{
	msg;
}

void PCMFileSource::OnRTCPEvent(void* param, const struct rtcp_msg_t* msg)
{
	PCMFileSource *self = (PCMFileSource *)param;
	self->OnRTCPEvent(msg);
}

int PCMFileSource::SendRTCP()
{
	// make sure have sent RTP packet

	time64_t clock = time64_now();
	int interval = rtp_rtcp_interval(m_rtp);
	if (0 == m_rtcp_clock || m_rtcp_clock + interval < clock)
	{
		char rtcp[1024] = { 0 };
		size_t n = rtp_rtcp_report(m_rtp, rtcp, sizeof(rtcp));

		// send RTCP packet
		m_transport->Send(true, rtcp, n);

		m_rtcp_clock = clock;
	}

	return 0;
}

void* PCMFileSource::RTPAlloc(void* param, int bytes)
{
	PCMFileSource *self = (PCMFileSource*)param;
	assert(bytes <= sizeof(self->m_packet));
	return self->m_packet;
}

void PCMFileSource::RTPFree(void* param, void *packet)
{
	PCMFileSource *self = (PCMFileSource*)param;
	assert(self->m_packet == packet);
}

void PCMFileSource::RTPPacket(void* param, const void *packet, int bytes, uint32_t /*timestamp*/, int /*flags*/)
{
	PCMFileSource *self = (PCMFileSource*)param;
	assert(self->m_packet == packet);

	int r = self->m_transport->Send(false, packet, bytes);
	assert(r == (int)bytes);
	rtp_onsend(self->m_rtp, packet, bytes/*, time*/);
}
