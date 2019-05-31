#include "tcpip/h/network.h"

/*------------------------------------------------------------------------
 *  ripok  -  determine if a received RIP route is ok to install
 *------------------------------------------------------------------------
 */
int ripok(struct riprt *rp)
{
	unsigned char net;

	if (rp->rr_family != AF_INET)
		return 0;
	if (rp->rr_metric > RIP_INFINITY)
		return 0;
	if (IP_CLASSD(rp->rr_ipa) || IP_CLASSE(rp->rr_ipa))
		return 0;
	net = (ntohl(rp->rr_ipa) & 0xff000000) >> 24;
	if (net == 0 && rp->rr_ipa != ip_anyaddr)
		return 0; /* net 0, host non-0		*/
	if (net == 127)
		return 0; /* loopback net			*/
	return 1;
}
