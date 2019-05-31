#include "tcpip/h/network.h"

#ifdef Ntcp
struct tcb tcbtab[Ntcp]; /* tcp device control blocks	*/
#endif

static bool tcpinited = 0;

/*------------------------------------------------------------------------
 *  tcpinit  -  devtab中tcp设备初始化
 *------------------------------------------------------------------------
 */
int tcpinit(struct device *pdev)
{
	struct tcb *tcb;

	if (tcpinited == 0)
	{
		tcpinited = 1;
		mutex_init(&tcps_tmutex);
		tcps_lqsize = 5; /* default listen Q size */
	}
	pdev->dvioblk = (char *)(tcb = &tcbtab[pdev->dvminor]);
	tcb->tcb_dvnum = pdev->dvnum;
	tcb->tcb_state = TCPS_FREE;
	return OK;
}
