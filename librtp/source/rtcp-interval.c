// RFC3550 A.7 Computing the RTCP Transmission Interval (p74)

#define _CRT_RAND_S
#include <stdlib.h>

#if defined(OS_WINDOWS)
#include <limits.h>
double drand48(void)
{
	unsigned int v = 0;
	rand_s(&v);
	return (v * 1.0) / UINT_MAX;
}
#endif

double rtcp_interval(int members,
					int senders,
					double rtcp_bw,
					int we_sent,
					double avg_rtcp_size,
					int initial)
{
	/*
	* Minimum average time between RTCP packets from this site (in
	* seconds). This time prevents the reports from `clumping' when
	* sessions are small and the law of large numbers isn't helping
	* to smooth out the traffic. It also keeps the report interval
	* from becoming ridiculously small during transient outages like
	* a network partition.
	*/
	double const RTCP_MIN_TIME = 5.0;

	/*
	* Fraction of the RTCP bandwidth to be shared among active
	* senders. (This fraction was chosen so that in a typical
	* session with one or two active senders, the computed report
	* time would be roughly equal to the minimum report time so that
	* we don't unnecessarily slow down receiver reports.) The
	* receiver fraction must be 1 - the sender fraction.
	*/
	double const RTCP_SENDER_BW_FRACTION = 0.25;
	double const RTCP_RCVR_BW_FRACTION = (1-RTCP_SENDER_BW_FRACTION);

	/*
	* To compensate for "timer reconsideration" converging to a
	* value below the intended average.
	*/
	double const COMPENSATION = 2.71828 - 1.5;
	double t; /* interval */
	double rtcp_min_time = RTCP_MIN_TIME;
	int n; /* no. of members for computation */

	/*
	* Very first call at application start-up uses half the min
	* delay for quicker notification while still allowing some time
	* before reporting for randomization and to learn about other
	* sources so the report interval will converge to the correct
	* interval more quickly.
	*/
	if (initial) {
		rtcp_min_time /= 2;
	}

	/*
	* Dedicate a fraction of the RTCP bandwidth to senders unless
	* the number of senders is large enough that their share is
	* more than that fraction.
	*/
	n = members;
	if (senders <= members * RTCP_SENDER_BW_FRACTION) {
		if (we_sent) {
			rtcp_bw *= RTCP_SENDER_BW_FRACTION;
			n = senders;
		} else {
			rtcp_bw *= RTCP_RCVR_BW_FRACTION;
			n -= senders;
		}
	}

	/*
	* The effective number of sites times the average packet size is
	* the total number of octets sent when each site sends a report.
	* Dividing this by the effective bandwidth gives the time
	* interval over which those packets must be sent in order to
	* meet the bandwidth target, with a minimum enforced. In that
	* time interval we send one report so this time is also our
	* average time between reports.
	*/
	t = avg_rtcp_size * n / rtcp_bw;
	if (t < rtcp_min_time) t = rtcp_min_time;

	/*
	* To avoid traffic bursts from unintended synchronization with
	* other sites, we then pick our actual next report interval as a
	* random number uniformly distributed between 0.5*t and 1.5*t.
	*/
	t = t * (drand48() + 0.5);
	t = t / COMPENSATION;
	return t;
}
