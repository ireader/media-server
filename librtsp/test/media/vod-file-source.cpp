#include "vod-file-source.h"
#include "sys/system.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

VodFileSource::VodFileSource(std::shared_ptr<IFileReader> reader, std::shared_ptr<AVPacketQueue> pkts)
	:m_status(STOP), m_running(true), m_speed(128), m_clock(0), m_timestamp(0), m_reader(reader), m_avpkts(pkts)
{
	thread_create(&m_thread, Worker, this);
}

VodFileSource::~VodFileSource()
{
	m_running = false;
	m_event.Signal();
	thread_destroy(m_thread);
}

int VodFileSource::Worker(void* param)
{
	VodFileSource* s = (VodFileSource*)param;
	return s->Worker();
}

int VodFileSource::Worker()
{
	int r = 0;
	struct avpacket_t* pkt = NULL;

	do 
	{
		while ( (m_status == STOP || m_status == PAUSE) && m_running)
			m_event.Wait();

		while (m_status == PLAY && 0 == r && m_running)
		{
			if (!pkt)
			{
				AutoThreadLocker locker(m_locker);
				r = m_reader->Read(&pkt);
				if (r <= 0)
					break; // all done
			}

			uint64_t now = system_clock();
			uint64_t timestamp = 0 == pkt->dts || -1 == pkt->dts ? pkt->pts : pkt->dts;
			
			if (0 == m_clock || now < m_clock || timestamp < m_timestamp)
			{
				m_clock = now;
				m_timestamp = timestamp;
			}

			int64_t diff = (int64_t)((timestamp - m_timestamp) - ((now - m_clock) * m_speed / 128));
			if (diff <= 0 || m_event.TimeWait((int)(diff * 128 / m_speed)))
			{
                r = m_avpkts->Push(pkt);
				avpacket_release(pkt);
				pkt = NULL; // reset
			}
		}
	} while (0 == r && m_running);

	avpacket_release(pkt);
	return 0;
}

int VodFileSource::Play()
{
	if (PLAY != m_status)
	{
		m_clock = 0;
		m_timestamp = 0;
		m_status = PLAY;
		m_event.Signal();
	}
	return 0;
}

int VodFileSource::Pause()
{
	m_status = PAUSE;
	m_event.Signal();
	return 0;
}

int VodFileSource::Seek(uint64_t* pos, int strategy)
{
	AutoThreadLocker locker(m_locker);
	return m_reader->Seek(pos, strategy);
}

int VodFileSource::GetSpeed() const
{
	return m_speed;
}

int VodFileSource::SetSpeed(int speed)
{
	m_speed = speed;
	return 0;
}

uint64_t VodFileSource::GetPosition() const
{
	return m_reader->GetPosotion();
}

uint64_t VodFileSource::GetDuration() const
{
	return m_reader->GetDuration();
}
