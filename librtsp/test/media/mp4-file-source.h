#pragma once

#include "media-source.h"
#include "mov-reader.h"
#include <string>
#include <stdio.h>
#include <stdint.h>

class MP4FileSource : public IMediaSource
{
public:
	MP4FileSource(const char *file);
	virtual ~MP4FileSource();

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
	struct media_t;
	static void OnRTCPEvent(void* param, const struct rtcp_msg_t* msg);
	void OnRTCPEvent(const struct rtcp_msg_t* msg);
	int SendRTCP(struct media_t* m, uint64_t clock);
	int SendBye();

	static void* RTPAlloc(void* param, int bytes);
	static void RTPFree(void* param, void *packet);
	static void RTPPacket(void* param, const void *packet, int bytes, uint32_t timestamp, int flags);

	static void MP4OnVideo(void* param, uint32_t track, uint8_t object, int width, int height, const void* extra, size_t bytes);
	static void MP4OnAudio(void* param, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes);
	static void MP4OnRead(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts);

private:
	int64_t m_dts;
	uint64_t m_clock;
	mov_reader_t* m_reader;
	FILE* m_fp;

	std::string m_sdp;

	int m_status;
	int64_t m_pos;
	double m_speed;

	struct frame_t
	{
		uint8_t buffer[2 * 1024 * 1024];
		size_t bytes;
		int64_t pts;
		int64_t dts;
		int track;
	};
	struct frame_t m_frame;

	struct media_t
	{
		void* rtp;
		int64_t dts_first; // first frame timestamp
		int64_t dts_last; // last frame timestamp
		uint64_t timestamp; // rtp timestamp
		uint64_t rtcp_clock;

		uint32_t ssrc;
		int bandwidth;
		int frequency;
		char name[64];
		int payload;
		void* packer; // rtp encoder
		uint8_t packet[1450];

		std::shared_ptr<IRTPTransport> transport;
		int track; // mp4 track
	};

	struct media_t m_media[2]; // 0-video, 1-audio
	int m_count;
};
