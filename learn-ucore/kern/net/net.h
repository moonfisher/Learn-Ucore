#ifndef __KERN_NET_NET_H__
#define __KERN_NET_NET_H__

#include "sockets.h"
#include "mmu.h"
#include "sem.h"
#include "mutex.h"

extern mutex network_mtx;

void start_net_mechanics(void);

#endif // !__KERN_NET_NET_H__
