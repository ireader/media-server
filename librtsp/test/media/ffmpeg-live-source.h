#pragma once

#if defined(_HAVE_FFMPEG_)
extern "C" {
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
}
#include "media-source.h"
#include <string>
#include <stdint.h>

class FFLiveSource : public IMediaSource
{
public:
	FFLiveSource(const char *source);
	virtual ~FFLiveSource();

public:
	virtual int Play();
	virtual int Pause();
	virtual int Seek(int64_t pos);
	virtual int SetSpeed(double speed);
	virtual int GetDuration(int64_t& duration) const;
	virtual int GetSDPMedia(std::string& sdp) const;
	virtual int GetRTPInfo(const char* uri, char *rtpinfo, size_t bytes) const;
	virtual int SetTransport(const char* track, std::shared_ptr<IRTPTransport> transport);

private:
	int Open(const char* file);

	struct media_t;
	static void OnRTCPEvent(void* param, const struct rtcp_msg_t* msg);
	void OnRTCPEvent(const struct rtcp_msg_t* msg);
	static int SendRTCP(struct media_t* m, uint64_t clock);
	int SendBye();

	static void* RTPAlloc(void* param, int bytes);
	static void RTPFree(void* param, void *packet);
	static void RTPPacket(void* param, const void *packet, int bytes, uint32_t timestamp, int flags);

	void OnVideo(AVCodecParameters* codecpar);
	void OnAudio(AVCodecParameters* codecpar);

private:
	int64_t m_dts;
	uint64_t m_clock;

	std::string m_sdp;

	int m_status;
	int64_t m_pos;
	double m_speed;

	AVFormatContext* m_ic;
	AVBSFContext* m_h264bsf;
	AVPacket m_pkt;

	struct media_t
	{
		AVCodecContext* encoder;
		AVCodecContext* decoder;

		void* rtp;
		int64_t dts_first; // first frame dts
		int64_t dts_last; // last frame dts
		uint64_t timestamp; // rtp timestamp
		uint64_t rtcp_clock;

		uint32_t ssrc;
		int bandwidth;
		int frequency;
		char name[64];
		int payload;
		void* packer; // rtp encoder
		uint8_t packet[2048];

		std::shared_ptr<IRTPTransport> transport;

		int track; // mp4 track
	};

	struct media_t m_media[2]; // 0-video, 1-audio
	int m_count;
};

#endif
