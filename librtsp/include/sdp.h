#ifndef _rfc4566_sdp_h_
#define _rfc4566_sdp_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sdp_t sdp_t;

enum { SDP_V_VERSION_0 = 0 };
enum { SDP_C_NETWORK_UNKNOWN=0, SDP_C_NETWORK_IN };
enum { SDP_C_ADDRESS_UNKNOWN=0, SDP_C_ADDRESS_IP4, SDP_C_ADDRESS_IP6 };
enum { SDP_A_SENDRECV = 0, SDP_A_SENDONLY, SDP_A_RECVONLY, SDP_A_INACTIVE, };
enum { SDP_M_FMT_UDP_AUDIO = 1001, SDP_M_FMT_UDP_VIDEO, SDP_M_FMT_UDP_TEXT, SDP_M_FMT_UDP_APPLICATION, SDP_M_FMT_UDP_MESSAGE };

sdp_t* sdp_parse(const char* s);
void sdp_destroy(sdp_t* sdp);

int sdp_version_get(sdp_t* sdp);

/// 5.2. Origin ("o=")
/// @param[out] username username
/// @param[out] session session id
/// @param[out] version session version
/// @param[out] network network type, IN-internet
/// @param[out] addrtype address type, IP4-IP v4, IP6-IP v6
/// @param[out] address connection address, multicast/unicast address
/// @return 0-ok, -1 if don't have connection
int sdp_origin_get(sdp_t* sdp, const char **username, const char** session, const char** version, const char** network, const char** addrtype, const char** address);
int sdp_origin_get_network(sdp_t* sdp); // SDP_C_NETWORK_IN
int sdp_origin_get_addrtype(sdp_t* sdp); // SDP_C_ADDRESS_IP4/SDP_C_ADDRESS_IP6

/// @return NULL-if don't have keyword
const char* sdp_session_get_name(sdp_t* sdp);
const char* sdp_session_get_information(sdp_t* sdp);
const char* sdp_uri_get(sdp_t* sdp);

int sdp_email_count(sdp_t* sdp);
int sdp_phone_count(sdp_t* sdp);
const char* sdp_email_get(sdp_t* sdp, int idx);
const char* sdp_phone_get(sdp_t* sdp, int idx);

// c=IN IP4 224.2.36.42/127
// c=IN IP4 224.2.1.1/127/3
// c=IN IP6 FF15::101/3
/// @param[out] network network type, IN-internet
/// @param[out] addrtype address type, IP4-IP v4, IP6-IP v6
/// @param[out] address connection address, multicast/unicast address
/// @return 0-ok, -1 if don't have connection
int sdp_connection_get(sdp_t* sdp, const char** network, const char** addrtype, const char** address);
int sdp_connection_get_address(sdp_t* sdp, char* ip, int bytes); // ipv4/ipv6 address, alloc by caller
int sdp_connection_get_network(sdp_t* sdp); // SDP_C_NETWORK_IN
int sdp_connection_get_addrtype(sdp_t* sdp); // SDP_C_ADDRESS_IP4/SDP_C_ADDRESS_IP6

int sdp_bandwidth_count(sdp_t* sdp);
const char* sdp_bandwidth_get_type(sdp_t* sdp, int idx); // CT/AS
int sdp_bandwidth_get_value(sdp_t* sdp, int idx); // kbps-kilobits per second

// 1. These values are the decimal representation of Network Time Protocol (NTP) time values in seconds
//    since 1900 [13]. To convert these values to UNIX time, subtract decimal 2208988800.
// 2. If the <stop-time> is set to zero, then the session is not bounded, though it will not become active 
//    until after the <start-time>. If the <start-time> is also zero, the session is regarded as permanent.
int sdp_timing_count(sdp_t* sdp);
int sdp_timing_repeat_count(sdp_t* sdp, int time);
int sdp_timing_repeat_offset_count(sdp_t* sdp, int time);
int sdp_timing_timezone_count(sdp_t* sdp, int time);
int sdp_timing_get(sdp_t* sdp, int idx, const char** start, const char** stop);
const char* sdp_repeat_get(sdp_t* sdp, int idx);
const char* sdp_timezone_get(sdp_t* sdp, int idx);

/// @return NULL-if don't have keyword
const char* sdp_encryption_get(sdp_t* sdp);

int sdp_media_count(sdp_t* sdp);
const char* sdp_media_type(sdp_t* sdp, int media);
int sdp_media_port(sdp_t* sdp, int media, int port[], int num); // return port count
const char* sdp_media_proto(sdp_t* sdp, int media);
int sdp_media_formats(sdp_t* sdp, int media, int *formats, int count); // return format count

int sdp_media_get_connection_address(sdp_t* sdp, int media, char* ip, int bytes);
int sdp_media_get_connection_network(sdp_t* sdp, int media);
int sdp_media_get_connection_addrtype(sdp_t* sdp, int media);
const char* sdp_media_attribute_find(sdp_t* sdp, int media, const char* name);
int sdp_media_attribute_list(sdp_t* sdp, int media, const char* name, void (*onattr)(void* param, const char* name, const char* value), void* param);

const char* sdp_media_get_information(sdp_t* sdp, int media);

int sdp_media_bandwidth_count(sdp_t* sdp, int media);
const char* sdp_media_bandwidth_get_type(sdp_t* sdp, int media, int idx); // CT/AS
int sdp_media_bandwidth_get_value(sdp_t* sdp, int media, int idx); // kbps-kilobits per second

int sdp_attribute_count(sdp_t* sdp);
int sdp_attribute_list(sdp_t* sdp, const char* name, void (*onattr)(void* param, const char* name, const char* value), void* param);
int sdp_attribute_get(sdp_t* sdp, int idx, const char** name, const char** value);
const char* sdp_attribute_find(sdp_t* sdp, const char* name);

/// "sendrecv" SHOULD be assumed as the default for sessions that 
/// are not of the conference type "broadcast" or "H332" (see below).
/// return -1-not found, SDP_A_SENDRECV/SDP_A_SENDONLY/SDP_A_XXX
int sdp_media_mode(struct sdp_t* sdp, int media);

#ifdef __cplusplus
}
#endif
#endif /* !_sdp_h_ */
