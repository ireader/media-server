#ifndef _rtmp_internal_h_
#define _rtmp_internal_h_

#include "rtmp-message.h"

#define N_CHUNK_STREAM	10 // maximum chunk stream count
#define N_TRANSACTIONS	10

enum rtmp_command_t
{
	RTMP_NETCONNECTION_CONNECT,
	RTMP_NETCONNECTION_CREATE_STREAM,

	RTMP_NETSTREAM_PLAY,
	RTMP_NETSTREAM_DELETE_STREAM,
	RTMP_NETSTREAM_CLOSE_STREAM,
	RTMP_NETSTREAM_RECEIVE_AUDIO,
	RTMP_NETSTREAM_RECEIVE_VIDEO,
	RTMP_NETSTREAM_PUBLISH,
	RTMP_NETSTREAM_SEEK,
	RTMP_NETSTREAM_PAUSE,

	RTMP_NETSTREAM_FCPUBLISH,
	RTMP_NETSTREAM_FCUNPUBLISH,
	RTMP_NETSTREAM_FCSUBSCRIBE,
	RTMP_NETSTREAM_FCUNSUBSCRIBE,
};

enum rtmp_notify_t
{
	RTMP_NOTIFY_START = 1,
	RTMP_NOTIFY_STOP,
	RTMP_NOTIFY_PAUSE,
	RTMP_NOTIFY_SEEK,
};

struct rtmp_transaction_t
{
	uint32_t id; // transaction id
	enum rtmp_command_t command;
};

struct rtmp_packet_t
{
	struct rtmp_chunk_header_t header;
	uint32_t timestamp;
	uint8_t* payload;

	size_t capacity; // only for network read
	size_t bytes; // only for network read
};

// 5.3.1. Chunk Format (p11)
/* 3-bytes basic header + 11-bytes message header + 4-bytes extended timestamp */
#define MAX_CHUNK_HEADER 18

enum rtmp_state_t
{
	RTMP_STATE_INIT = 0,
	RTMP_STATE_BASIC_HEADER,
	RTMP_STATE_MESSAGE_HEADER,
	RTMP_STATE_EXTENDED_TIMESTAMP,
	RTMP_STATE_PAYLOAD,
};

struct rtmp_parser_t
{
	uint8_t buffer[MAX_CHUNK_HEADER];
	uint32_t basic_bytes; // basic header length
	uint32_t bytes;

	enum rtmp_state_t state;
};

struct rtmp_t
{
	uint32_t in_chunk_size; // read from network
	uint32_t out_chunk_size; // write to network

	// 7.2.2.6. publish
	char playpath[256]; // Publishing name
	char stream_type[16]; // Publishing type: live/record/append

	double milliSeconds; // 7.2.2.7. seek

	double stream_id;
	uint32_t sequence_number; // bytes read report
	uint32_t window_size; // server bandwidth (2500000)
	uint32_t peer_bandwidth; // client bandwidth
	
	uint32_t buffer_length_ms; // s -> c

	uint8_t limit_type; // client bandwidth limit
	uint8_t receiveAudio;
	uint8_t receiveVideo;
	uint8_t pause; // 7.2.2.8. pause

	uint32_t transaction_id;
	struct rtmp_transaction_t transactions[N_TRANSACTIONS]; // deprecate

	struct rtmp_result_base_t
	{
		char code[64]; // NetStream.Play.Start
		char level[16]; // warning/status/error
		char description[128];
	} result;

	// chunk header
	struct rtmp_packet_t in_packets[N_CHUNK_STREAM]; // receive from network
	struct rtmp_chunk_header_t out_headers[N_CHUNK_STREAM]; // send to network
	
	struct rtmp_parser_t parser;

	void* param;

	int (*send)(void* param, const uint8_t* header, uint32_t headerBytes, const uint8_t* payload, uint32_t payloadBytes);

	void (*onerror)(void* param, int code, const char* msg);

	union
	{
		struct
		{
			// server side
			int (*onhandler)(void* param, enum rtmp_command_t command);
		} server;

		struct
		{
			// client side
			void (*onconnect)(void* param);
			void (*oncreate_stream)(void* param, uint32_t stream_id);
			void (*onnotify)(void* param, enum rtmp_notify_t notify);
			void (*onping)(void* param, uint32_t timestamp); // send pong
			void (*onbandwidth)(void* param); // send window acknowledgement size
			void (*onaudio)(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp);
			void (*onvideo)(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp);
		} client;
	} u;
};

int rtmp_chunk_send(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* payload);
int rtmp_chunk_input(struct rtmp_t* rtmp, const uint8_t* data, size_t bytes);

int rtmp_message_send(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* payload);

int rtmp_handler(struct rtmp_t* rtmp, struct rtmp_chunk_header_t* header, const uint8_t* payload);
int rtmp_event_handler(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* data);
int rtmp_invoke_handler(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* data);
int rtmp_control_handler(struct rtmp_t* rtmp, const struct rtmp_chunk_header_t* header, const uint8_t* data);

int rtmp_command_transaction_save(struct rtmp_t* rtmp, uint32_t transaction, enum rtmp_command_t command);

#endif /* !_rtmp_internal_h_ */
