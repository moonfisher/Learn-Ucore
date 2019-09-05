#ifndef __KERN_FS_IOBUF_H__
#define __KERN_FS_IOBUF_H__

#include "defs.h"

/*
 * iobuf is a buffer Rd/Wr status record
 
 io_base:   它始终指向下一个未被使用的内存空间。所以在读写操作结束后，要对它的值（即指向的内存地址）
            做相应的维护
 io_offset: 表示信息所在地址的偏移量。如果是读文件时，就表示从文件的第几个字节开始读，
            写的时候表示从第几个字节开始写
 io_len:    一般表示待写的或者待读的字节数
 io_resid:  表示实际读或者写完后剩余的字节数。可以通过 io resid 是否为 0 来判断是否读写了规定的字节数。
 
 除 io_len 外另外 3 个成员都要在文件系统的函数里进行维护
 */
struct iobuf
{
    void *io_base;     // the base addr of buffer (used for Rd/Wr)
    off_t io_offset;   // current Rd/Wr position in buffer, will have been incremented by the amount transferred
    size_t io_len;     // the length of buffer  (used for Rd/Wr)
    size_t io_resid;   // current resident length need to Rd/Wr, will have been decremented by the amount transferred.
};

#define iobuf_used(iob)                         ((size_t)((iob)->io_len - (iob)->io_resid))

struct iobuf *iobuf_init(struct iobuf *iob, void *base, size_t len, off_t offset);
int iobuf_move(struct iobuf *iob, void *data, size_t len, bool m2b, size_t *copiedp);
int iobuf_move_zeros(struct iobuf *iob, size_t len, size_t *copiedp);
void iobuf_skip(struct iobuf *iob, size_t n);

#endif /* !__KERN_FS_IOBUF_H__ */

