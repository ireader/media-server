#ifndef _avpacket_queue_h_
#define _avpacket_queue_h_

#include "avpacket.h"
#include <stdint.h>

struct avpacket_queue_t;

struct avpacket_queue_t* avpacket_queue_create(int size);
void avpacket_queue_destroy(struct avpacket_queue_t* q);

void avpacket_queue_clear(struct avpacket_queue_t* q);
int avpacket_queue_count(struct avpacket_queue_t* q);
int avpacket_queue_pop(struct avpacket_queue_t* q);

struct avpacket_t* avpacket_queue_front(struct avpacket_queue_t* q);
int avpacket_queue_push(struct avpacket_queue_t* q, struct avpacket_t* pkt);

struct avpacket_t* avpacket_queue_front_wait(struct avpacket_queue_t* q, int ms);
int avpacket_queue_push_wait(struct avpacket_queue_t* q, struct avpacket_t* pkt, int ms);

struct avpacket_t* avpacket_queue_cur(struct avpacket_queue_t* q);
bool avpacket_queue_end(struct avpacket_queue_t* q);
void avpacket_queue_reset(struct avpacket_queue_t* q);

#if defined(__cplusplus)
class AVPacketQueue
{
public:
	AVPacketQueue(int size) :m_pkts(avpacket_queue_create(size)) {}
	~AVPacketQueue() { if (m_pkts) avpacket_queue_destroy(m_pkts); }

public:
	void Clear() { avpacket_queue_clear(m_pkts); }
	int Count() const { return avpacket_queue_count(m_pkts); }

	int Pop() { return avpacket_queue_pop(m_pkts); }
	int Push(struct avpacket_t* pkt) { return avpacket_queue_push(m_pkts, pkt); }
	int PushWait(struct avpacket_t* pkt, int ms) { return avpacket_queue_push_wait(m_pkts, pkt, ms); }
	struct avpacket_t* Front() { return avpacket_queue_front(m_pkts); }
	struct avpacket_t* FrontWait(int ms) { return avpacket_queue_front_wait(m_pkts, ms); }

	struct avpacket_t* Cur() { return avpacket_queue_cur(m_pkts); };
	bool End() { return avpacket_queue_end(m_pkts); };
	void Reset() { return avpacket_queue_reset(m_pkts); };

private:
	struct avpacket_queue_t* m_pkts;
};
#endif
#endif /* !_avpacket_queue_h_*/
