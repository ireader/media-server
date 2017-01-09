#ifndef _rtmp_msgtypeid_h_
#define _rtmp_msgtypeid_h_

enum RTMPMessageTypeId
{
	/* Protocol Control Messages */
	RTMP_TYPE_SET_CHUNK_SIZE = 1,
	RTMP_TYPE_ABORT = 2,
	RTMP_TYPE_ACKNOWLEDGEMENT = 3,
	RTMP_TYPE_WINDOW_ACKNOWLEDGEMENT_SIZE = 5,
	RTMP_TYPE_SET_PEER_BANDWIDTH = 6,

	/* User Control Messages (4) */
	RTMP_TYPE_PING = 4,

	RTMP_TYPE_AUDIO = 8,
	RTMP_TYPE_VIDEO = 9,
	
	/* Data Message */
	RTMP_TYPE_DATA_AMF3 = 15,
	RTMP_TYPE_DATA_AMF0 = 18,

	/* Shared Object Message */
	RTMP_TYPE_SHARED_OBJECT_AMF3 = 16,
	RTMP_TYPE_SHARED_OBJECT_AMF0 = 19,

	/* Command Message */
	RTMP_TYPE_COMMAND_AMF3 = 17,
	RTMP_TYPE_COMMAND_AMF0 = 20,

	/* Aggregate Message */
	RTMP_TYPE_AGGREGATE = 22,
};

#endif /* !_rtmp_msgtypeid_h_ */
