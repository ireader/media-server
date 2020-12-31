#ifndef _vod_file_source_h_
#define _vod_file_source_h_

#include "avpacket-queue.h"
#include "cpm/shared_ptr.h"
#include "sys/thread.h"
#include "sys/sync.hpp"

class VodFileSource
{
public:
	struct IFileReader
	{
		virtual ~IFileReader() {}
		virtual int Read(struct avpacket_t** pkt) = 0;

		/// @param[inout] pos input-seek position, output-location position
		/// @param[in] strategy
		/// @return 0-ok, other-error
		virtual int Seek(uint64_t* pos, int strategy) = 0;

		virtual uint64_t GetPosotion() = 0;
		virtual uint64_t GetDuration() = 0;
	};

public:
	VodFileSource(std::shared_ptr<IFileReader> reader, std::shared_ptr<AVPacketQueue> pkts);
	~VodFileSource();

public:
	int Play();
	int Pause();
	/// @param[inout] pos input-seek position, output-location position
	/// @param[in] strategy
	/// @return 0-ok, other-error
	int Seek(uint64_t* pos, int strategy);
	int GetSpeed() const;
	int SetSpeed(int speed);
	uint64_t GetPosition() const;
	uint64_t GetDuration() const;

private:
	int Worker();
	static int STDCALL Worker(void* param);

private:
	enum { STOP, PLAY, PAUSE };
	volatile int m_status;
	volatile bool m_running;

	int m_speed;
	uint64_t m_clock; // wall-clock
	uint64_t m_timestamp; // av packet pts/dts

	pthread_t m_thread;
	ThreadEvent m_event;
	ThreadLocker m_locker;
	std::shared_ptr<IFileReader> m_reader;
	std::shared_ptr<AVPacketQueue> m_avpkts;
};

#endif /* _vod_file_source_h_ */
