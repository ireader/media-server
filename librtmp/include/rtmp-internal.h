#ifndef _rtmp_internal_h_
#define _rtmp_internal_h_

#include "rtmp-message.h"

#define N_CHUNK_STREAM 10 // maximum chunk stream count

struct rtmp_t
{
	uint32_t chunk_size;
	uint32_t peer_chunk_size;

	// 7.2.2.6. publish
	char stream_name[256]; // Publishing name
	char stream_type[16]; // Publishing type: live/record/append

	double milliSeconds; // 7.2.2.7. seek

	double stream_id;
	uint32_t sequence_number;
	uint32_t window_size;
	uint32_t peer_bandwidth;
	
	uint32_t buffer_length_ms; // s -> c

	uint8_t limit_type;
	uint8_t receiveAudio;
	uint8_t receiveVideo;
	uint8_t pause; // 7.2.2.8. pause

	int transaction_id;

	struct rtmp_result_base_t
	{
		char code[64]; // NetStream.Play.Start
		char level[16]; // warning/status/error
		char description[128];
	} result;

	// saved chunk header
	struct rtmp_chunk_header_t headers[N_CHUNK_STREAM];

	void* param;

	int (*send)(void* param, const uint8_t* header, uint32_t headerBytes, const uint8_t* payload, uint32_t payloadBytes);
	int (*onpacket)(void* param, struct rtmp_chunk_header_t* header, const uint8_t* payload);
	int (*onping)(void* param, uint32_t timestamp);
	int (*onbandwidth)(void* param);
};

int rtmp_chunk_send(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* payload);
int rtmp_chunk_input(struct rtmp_t* rtmp, const uint8_t* data, size_t bytes);

int rtmp_notify_handler(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* data);
int rtmp_control_handler(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* data);

#endif /* !_rtmp_internal_h_ */
