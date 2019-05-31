#include "tcpip/h/network.h"

/*------------------------------------------------------------------------
 *  ipfadd  -  add a fragment to an IP fragment queue
 *------------------------------------------------------------------------
 */
bool ipfadd(struct ipfq *iq, struct ep *pep)
{
	struct ip *pip;
	int fragoff;

	if (iq->ipf_state != IPFF_VALID)
	{
		freebuf(pep);
		return 0;
	}
	pip = (struct ip *)pep->ep_data;
	fragoff = pip->ip_fragoff & IP_FRAGOFF;

	if (enq(iq->ipf_q, pep) == 0)
	{
		/* overflow-- free all frags and drop */
		freebuf(pep);
		IpReasmFails++;
		while ((pep = (struct ep *)deq(iq->ipf_q)))
		{
			freebuf(pep);
			IpReasmFails++;
		}
		assert(is_emptyq(iq->ipf_q));
		iq->ipf_state = IPFF_BOGUS;
		return 0;
	}
	iq->ipf_ttl = IP_FTTL; /* restart timer */
	return 1;
}
