#ifndef _rtsp_server_h_
#define _rtsp_server_h_

#include "rtsp-header-transport.h"
#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct rtsp_server_t rtsp_server_t;

struct rtsp_handler_t
{
	/// rtsp_server_destroy will call this function
	/// @param[in] ptr2 user-defined parameter
	int (*close)(void* ptr2);

	/// Network send
	/// @param[in] ptr2 user-defined parameter
	/// @param[in] data send data
	/// @param[in] bytes data length in byte
	/// @return 0-ok, other-error
	int (*send)(void* ptr2, const void* data, size_t bytes);

	/// RTSP DESCRIBE request(call rtsp_server_reply_describe)
	/// @param[in] ptr user-defined parameter
	/// @param[in] uri request uri
	/// @return 0-ok, other-error
	int (*ondescribe)(void* ptr, rtsp_server_t* rtsp, const char* uri);

	/// RTSP SETUP request(call rtsp_server_reply_setup)
	/// @param[in] ptr user-defined parameter
	/// @param[in] uri request uri
	/// @param[in] session RTSP Session
	/// @param[in] transport RTSP Transport header
	/// @return 0-ok, other-error
	int (*onsetup)(void* ptr, rtsp_server_t* rtsp, const char* uri, const char* session, const struct rtsp_header_transport_t transports[], size_t num);

	/// RTSP PLAY request(call rtsp_server_reply_play)
	/// @param[in] ptr user-defined parameter
	/// @param[in] session RTSP Session
	/// @param[in] npt request begin time, NULL if don't have Range parameter, 0 represent now
	/// @param[in] scale request scale, NULL if don't have Scale parameter
	/// @return 0-ok, other-error code
	int (*onplay)(void* ptr, rtsp_server_t* rtsp, const char* uri, const char* session, const int64_t *npt, const double *scale);

	/// RTSP PAUSE request(call rtsp_server_reply_pause)
	/// @param[in] ptr user-defined parameter
	/// @param[in] session RTSP Session
	/// @param[in] npt request pause time, NULL if don't have Range parameter
	/// @return 0-ok, other-error code
	int (*onpause)(void* ptr, rtsp_server_t* rtsp, const char* uri, const char* session, const int64_t *npt);

	/// RTSP TEARDOWN request(call rtsp_server_reply_teardown)
	/// @param[in] ptr user-defined parameter
	/// @param[in] session RTSP Session
	/// @param[in] uri request uri
	/// @return 0-ok, other-error code
	int (*onteardown)(void* ptr, rtsp_server_t* rtsp, const char* uri, const char* session);

    /// RTSP ANNOUNCE request(call rtsp_server_reply_announce)
    /// @param[in] ptr user-defined parameter
    /// @param[in] uri request uri
    /// @param[in] sdp RTSP SDP
    /// @return 0-ok, other-error
    int (*onannounce)(void* ptr, rtsp_server_t* rtsp, const char* uri, const char* sdp);

    /// RTSP RECORD request(call rtsp_server_reply_record)
    /// @param[in] ptr user-defined parameter
    /// @param[in] session RTSP Session
    /// @param[in] npt request begin time, NULL if don't have Range parameter, 0 represent now
    /// @param[in] scale request scale, NULL if don't have Scale parameter
    /// @return 0-ok, other-error code
    int (*onrecord)(void* ptr, rtsp_server_t* rtsp, const char* uri, const char* session, const int64_t *npt, const double *scale);

	/// RTSP OPTIONS request
	/// @param[in] uri default is '*' 
	/// @return 0-ok, other-error code
	int (*onoptions)(void* ptr, rtsp_server_t* rtsp, const char* uri);

	/// RTSP GET_PARAMETER/SET_PARAMETER request
	/// use rtsp_server_get_header("content-type") to if need
	/// @param[in] uri the presentation/stream uri
	/// @param[in] session RTSP session
	/// @param[in] content paramter(s), NULL for test client or server liveness ("ping")
	/// @return 0-ok, other-error code
	int (*ongetparameter)(void* ptr, rtsp_server_t* rtsp, const char* uri, const char* session, const void* content, int bytes);
	int (*onsetparameter)(void* ptr, rtsp_server_t* rtsp, const char* uri, const char* session, const void* content, int bytes);
};

/// create (reuse-able) rtsp server
/// param[in] ip peer(client) ip(IPv4/IPv6)
/// param[in] port peer(client) port
/// param[in] handler callbacks
/// param[in] ptr callback(except send) parameter
/// param[in] ptr2 send callback parameter
/// @return NULL-error, other-rtsp server instance
rtsp_server_t* rtsp_server_create(const char ip[65], unsigned short port, struct rtsp_handler_t* handler, void* ptr, void* ptr2);

/// destroy rtsp server
/// @param[in] server rtsp server instance
/// @return 0-ok, other-error code
int rtsp_server_destroy(rtsp_server_t* server);

/// client request
/// @param[in] data rtsp request
/// @param[in,out] bytes input data length, output remain length
/// @return 0-ok, 1-need more data, other-error
int rtsp_server_input(rtsp_server_t* rtsp, const void* data, size_t* bytes);

/// RTSP DESCRIBE reply
/// @param[in] rtsp request handle
/// @param[in] code RTSP status-code(200-OK, 301-Move Permanently, ...)
/// @param[in] sdp RTSP SDP
/// @return 0-ok, other-error code
int rtsp_server_reply_describe(rtsp_server_t* rtsp, int code, const char* sdp);

/// RTSP SETUP reply
/// @param[in] rtsp request handle
/// @param[in] code RTSP status-code(200-OK, 301-Move Permanently, ...)
/// @param[in] session RTSP Session parameter
/// @param[in] transport RTSP Transport parameter
/// @return 0-ok, other-error code
int rtsp_server_reply_setup(rtsp_server_t* rtsp, int code, const char* session, const char* transport);

/// RTSP PLAY reply
/// @param[in] rtsp request handle
/// @param[in] code RTSP status-code(200-OK, 301-Move Permanently, ...)
/// @param[in] nptstart Range start time(ms) [optional]
/// @param[in] nptend Range end time(ms) [optional]
/// @param[in] rtpinfo RTP-info [optional] e.g. url=rtsp://foo.com/bar.avi/streamid=0;seq=45102,url=rtsp://foo.com/bar.avi/streamid=1;seq=30211
/// @return 0-ok, other-error code
int rtsp_server_reply_play(rtsp_server_t* rtsp, int code, const int64_t *nptstart, const int64_t *nptend, const char* rtpinfo);

/// RTSP PAUSE reply
/// @param[in] rtsp request handle
/// @param[in] code RTSP status-code(200-OK, 301-Move Permanently, ...)
/// @return 0-ok, other-error code
int rtsp_server_reply_pause(rtsp_server_t* rtsp, int code);

/// RTSP PAUSE reply
/// @param[in] rtsp request handle
/// @param[in] code RTSP status-code(200-OK, 301-Move Permanently, ...)
/// @return 0-ok, other-error code
int rtsp_server_reply_teardown(rtsp_server_t* rtsp, int code);

/// RTSP ANNOUNCE reply
/// @param[in] rtsp request handle
/// @param[in] code RTSP status-code(200-OK, 301-Move Permanently, ...)
/// @return 0-ok, other-error code
int rtsp_server_reply_announce(rtsp_server_t* rtsp, int code);

/// RTSP RECORD reply
/// @param[in] rtsp request handle
/// @param[in] code RTSP status-code(200-OK, 301-Move Permanently, ...)
/// @param[in] nptstart Range start time(ms) [optional]
/// @param[in] nptend Range end time(ms) [optional]
/// @return 0-ok, other-error code
int rtsp_server_reply_record(rtsp_server_t* rtsp, int code, const int64_t *nptstart, const int64_t *nptend);

/// RTSP OPTIONS reply
/// @return 0-ok, other-error code
int rtsp_server_reply_options(rtsp_server_t* rtsp, int code);

/// RTSP GET_PARAMETER reply(content-type/content-encoding/content-language copy from request header)
/// @param[in] rtsp request handle
/// @param[in] code RTSP status-code(200-OK...)
/// @return 0-ok, other-error code
int rtsp_server_reply_get_parameter(rtsp_server_t* rtsp, int code, const void* content, int bytes);

/// RTSP SET_PARAMETER reply
/// @param[in] rtsp request handle
/// @param[in] code RTSP status-code(200-OK...)
/// @return 0-ok, other-error code
int rtsp_server_reply_set_parameter(rtsp_server_t* rtsp, int code);

/// RTSP send Embedded (Interleaved) Binary Data
/// @param[in] rtsp request handle
/// @param[in] data interleaved binary data, start with 1-byte $ + 1-byte CHANNEL + 2-bytes LEN + RTP/RTCP HEADER + PAYLOAD
/// @param[in] bytes data length in bytes
/// @return 0-ok, other-error code
int rtsp_server_send_interleaved_data(rtsp_server_t* rtsp, const void* data, size_t bytes);

/// find RTSP header
/// @param[in] rtsp request handle
/// @param[in] name header name
/// @return header value, NULL if not found.
/// NOTICE: call in rtsp_handler_t callback only
const char* rtsp_server_get_header(rtsp_server_t* rtsp, const char* name);

/// get client ip/port
const char* rtsp_server_get_client(rtsp_server_t* rtsp, unsigned short* port);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtsp_server_h_ */
