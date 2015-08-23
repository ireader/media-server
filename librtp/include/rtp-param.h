#ifndef _rtp_param_h_
#define _rtp_param_h_

// RFC3550 6.2 RTCP Transmission Interval (p21)
// It is recommended that the fraction of the session bandwidth added for RTCP be fixed at 5%.
// It is also recommended that 1/4 of the RTCP bandwidth be dedicated to participants that are sending data
#define RTCP_BANDWIDTH_FRACTION			0.05
#define RTCP_SENDER_BANDWIDTH_FRACTION	0.25

#define RTCP_REPORT_INTERVAL			5000 /* milliseconds RFC3550 p25 */
#define RTCP_REPORT_INTERVAL_MIN		2500 /* milliseconds RFC3550 p25 */

#endif /* !_rtp_param_h_ */
