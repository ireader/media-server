#ifndef _rtmp_msgtypeid_h_
#define _rtmp_msgtypeid_h_

enum RTMPMessageTypeId
{
	RTMP_TYPE_PING = 0x04, /* User Control Messages (4) */

	RTMP_TYPE_AUDIO = 0x08,
	RTMP_TYPE_VIDEO = 0x09,
	RTMP_TYPE_AMF3 = 0x11,
	RTMP_TYPE_INVOKE = 0x12,
	RTMP_TYPE_AMF0 = 0x14,
};

#endif /* !_rtmp_msgtypeid_h_ */
