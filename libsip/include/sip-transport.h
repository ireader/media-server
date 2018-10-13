#ifndef _sip_transport_h_
#define _sip_transport_h_

#if defined(__cplusplus)
extern "C" {
#endif

// 19.1.1 SIP and SIPS URI Components (p152)
// The default port value is transport and scheme dependent. The
// default is 5060 for sip: using UDP, TCP, or SCTP. The default is
// 5061 for sip: using TLS over TCP and sips: over TCP.
#define SIP_PORT 5060

// 8.1.1.6 Max-Forwards (p39)
#define DEFAULT_MAX_FORWARS 70

// 17.1.1.1 Overview of INVITE Transaction (p125)
// T1 is an estimate of the round-trip time (RTT), and it defaults to 500 ms.
#define DEFAULT_RTT 500				// 500ms
#define T1			DEFAULT_RTT		// RTT Estimate
#define T2			(4 * 1000)		// 4s, The maximum retransmit interval for non-INVITE requests and INVITE responses
#define T4			(5 * 1000)		// 5s, Maximum duration a message will remain in the network

// A Table of Timer Values (p265)
#define TIMER_A		T1				// INVITE request retransmit interval, for UDP only
#define TIMER_B		(64 * T1)		// INVITE transaction timeout timer
#define TIMER_C		(3 * 60 * 1000) // >3min, proxy INVITE transaction timeout
#define TIMER_D		(32 * 1000)		// >32s, Wait time for response retransmits
#define TIMER_E		T1				// non-INVITE request retransmit interval, UDP only
#define TIMER_F		(64 * T1)		// non-INVITE transaction timeout timer
#define TIMER_G		T1				// INVITE response retransmit interval
#define TIMER_H		(64 * T1)		// Wait time for ACK receipt
#define TIMER_I		T4				// Wait time for ACK retransmits
#define TIMER_J		(64 * T1)		// Wait time for non-INVITE request retransmits
#define TIMER_K		T4				// Wait time for response retransmits
#define TIMER_L		(64 * T1)		// INVITE server accepted 
#define TIMER_M		(64 * T1)		// INVITE client accepted

struct sip_transport_t
{
	/// @param[in] destination remote host/addr
	/// @param[out] protocol UDP/TCP/TLS/SCTP
	/// @param[out] local local address, IPv4/IPv6 with port
	/// @param[out] dns local host dns(with port && with via maddr, ttl, and sent-by parameters)
	/// @return 0-ok, other-error
	int (*via)(void* transport, const char* destination, char protocol[16], char local[128], char dns[128]);

	/// @return 0-ok, other-error
	int (*send)(void* transport, const void* data, size_t bytes);
};

#if defined(__cplusplus)
}
#endif
#endif /* !_sip_transport_h_ */
