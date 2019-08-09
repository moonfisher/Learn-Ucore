#ifndef __KERN_SYNC_MBOX_H__
#define __KERN_SYNC_MBOX_H__

#include "defs.h"

void mbox_init(void);

struct mboxbuf;
struct mboxinfo;

/*
 进程间有大的数据需要共享和传输，采用 mbox 邮箱方式，邮箱内容没处理完时发送端会阻塞
 */
int ipc_mbox_init(unsigned int max_slots);
int ipc_mbox_send(int id, struct mboxbuf *buf, unsigned int timeout);
int ipc_mbox_recv(int id, struct mboxbuf *buf, unsigned int timeout);
int ipc_mbox_send_k(int id, struct mboxbuf *buf, unsigned int timeout);
int ipc_mbox_recv_k(int id, struct mboxbuf *buf, unsigned int timeout);
int ipc_mbox_free(int id);
int ipc_mbox_info(int id, struct mboxinfo *info);
int ipc_mbox_sender_count(int id);
int ipc_mbox_recver_count(int id);

void mbox_cleanup(void);

#endif /* !__KERN_SYNC_MBOX_H__ */
