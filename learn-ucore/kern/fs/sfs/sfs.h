#ifndef __KERN_FS_SFS_SFS_H__
#define __KERN_FS_SFS_SFS_H__

#include "defs.h"
#include "mmu.h"
#include "list.h"
#include "sem.h"
#include "unistd.h"

/*
 * Simple FS (SFS) definitions visible to ucore. This covers the on-disk format
 * and is used by tools that work on SFS volumes, such as mksfs.
 */
/*
 ucore 内核把所有文件都看作是字节流，任何内部逻辑结构都是专用的，由应用程序负责解释。
 ucore 区分文件的物理结构，目前 ucore 支持如下几种类型的文件：
 
 常规文件：文件中包括的内容信息是由应用程序输入。SFS 文件系统在普通文件上不强加任何内部结构，
         把其文件内容信息看作为字节。
 目录：包含一系列的 entry，每个 entry 包含文件名和指向与之相关联的索引节点（index node）的指针。
      目录是按层次结构组织的。
 链接文件：实际上一个链接文件是一个已经存在的文件的另一个可选择的文件名。
 设备文件：不包含数据，但是提供了一个映射物理设备（如串口、键盘等）到一个文件名的机制。
         可通过设备文件访问外围设备。
 管道：管道是进程间通讯的一个基础设施。管道缓存了其输入端所接受的数据，以便在管道输出端读的进程能一个
      先进先出的方式来接受数据。
 
 在 github 上的 ucore 教学操作系统中，主要关注的是常规文件、目录和链接中的 hardlink 的设计实现。
 SFS 文件系统中目录和常规文件具有共同的属性，而这些属性保存在索引节点中。
 SFS 通过索引节点来管理目录和常规文件，索引节点包含操作系统所需要的关于某个文件的关键信息，
 比如文件的属性、访问许可权以及其他控制信息都保存在索引节点中。可以有多个文件名指向一个索引节点。
*/
#define SFS_MAGIC                                   0x2f8dbe2a              /* magic number for sfs */
#define SFS_BLKSIZE                                 PGSIZE                  /* size of block */
#define SFS_NDIRECT                                 12                      /* # of direct blocks in inode */
#define SFS_MAX_INFO_LEN                            31                      /* max length of infomation */
#define SFS_MAX_FNAME_LEN                           FS_MAX_FNAME_LEN        /* max length of filename */
#define SFS_MAX_FILE_SIZE                           (1024UL * 1024 * 128)   /* max file size (128M) */
#define SFS_BLKN_SUPER                              0                       /* block the superblock lives in */
#define SFS_BLKN_ROOT                               1                       /* location of the root dir inode */
#define SFS_BLKN_FREEMAP                            2                       /* 1st block of the freemap */

/* # of bits in a block */
#define SFS_BLKBITS                                 (SFS_BLKSIZE * CHAR_BIT)

/* # of entries in a block */
#define SFS_BLK_NENTRY                              (SFS_BLKSIZE / sizeof(uint32_t))

/* file types */
#define SFS_TYPE_INVAL                              0       /* Should not appear on disk */
#define SFS_TYPE_FILE                               1
#define SFS_TYPE_DIR                                2
#define SFS_TYPE_LINK                               3

/*
 * On-disk superblock
 */
/*
 Ucore 文件系统主要包含四类主要的数据结构，在后面几个文件系统中，这几类数据结构会经常反复地出现：
 
 超级块（SuperBlock）：它主要从文件系统的全局角度描述特定文件系统的全局信息。
                     它的作用范围是整个 OS 空间。
 索引节点（inode）：它主要从文件系统的单个文件的角度它描述了文件的各种属性和数据所在位置。
                  它的作用范围是整个 OS 空间。
 目录项（dentry）：它主要从文件系统的文件路径的角度描述了文件路径中的特定目录。
                 它的作用范围是整个 OS 空间。
 文件（file）：它主要从进程的角度描述了一个进程在访问文件时需要了解的文件标识，文件读写的位置，文件
             引用情况等信息。它的作用范围是某一具体进程。
 
 ucore 使用的 sfs.img 文件是在 mksfs 里创建的
 
 文件系统通常保存在磁盘上，disk0 代表磁盘，用来存放一个 SFS 文件系统。磁盘的使用是以扇区为单位的，
 但是在文件系统中，一般按数据块来使用磁盘，在 sfs 中，我们以4k（8 个 sector，和 page 大小相等）为
 一个数据块

 ---------------------------------------------------------------------------
 superblock | root-dir inode | freemap | inode / File data / Dir data blocks
 ---------------------------------------------------------------------------
 
 第 0 个块（4K）是超级块（sfs.img 文件第一个 4K 字节, superblock struct sfs_super），
 文件系统中第一个块被称为超级块。这个块存放文件系统本身的结构信息。比如，超级块记录了每个区域的大小，
 超级块也存放未被使用的磁盘块的信息。它包含了关于文件系统的所有关键参数，当计算机被启动或文件系统被
 首次接触时，超级块的内容就会被装入内存。
 
 第 1 个块（4K）(sfs.img 文件第 2 个 4K 字节) 放了一个 root-dir 的 inode，用来记录根目录的
 相关信息。root-dir 是 SFS 文件系统的根结点，通过这个 root-dir 的 inode 信息就可以定位并查找到
 根目录下的所有文件信息。
 
 从第 2 个块开始，根据 SFS 中所有块的数量，记录块占用情况，用 1 个 bit 来表示一个块的占用和未被
 占用的情况。这个区域称为 SFS 的 freemap 区域，这将占用若干个块空间。为了更好地记录和管理 freemap
 区域，专门提供了文件 kern/fs/sfs/bitmap.c 来完成根据一个块号查找或设置对应的 bit 位的值。
 
 最后在剩余的磁盘空间中，存放了所有其他目录和文件的 inode 信息和内容数据信息。需要注意的是虽然
 inode 的大小小于一个块的大小（4k），但为了实现简单，每个 inode 都占用一个完整的 block。
 
 通常，一个文件占用的多个物理块在磁盘上是不连续存储的，因为如果连续存储，则经过频繁的删除、建立、
 移动文件等操作，最后磁盘上将形成大量的空洞，很快磁盘上将无空间可供使用。因此，必须提供一种方法
 将一个文件占用的多个逻辑块映射到对应的非连续存储的物理块上去.
*/
struct sfs_super
{
    // magic 代表一个魔数，其值为 0x2f8dbe2a，内核用它来检查磁盘镜像是否合法
    uint32_t magic;                                 /* magic number, should be SFS_MAGIC */
    // blocks 记录了 sfs 中 block 的数量，1 block =
    uint32_t blocks;                                /* # of blocks in fs */
    // unused_block 记录了 sfs 中还没有被使用的 block 数量，其中关于物理磁盘的管理与
    // 虚拟内存的管理十分类似，每次使用物理磁盘也会有一个类似于物理内存管理的分配算法。
    uint32_t unused_blocks;                         /* # of unused blocks in fs */
    // info 记录一个字符串 "simple file system"
    char info[SFS_MAX_INFO_LEN + 1];                /* infomation for sfs  */
};

/* inode (on disk 磁盘上的 inode 二进制结构) */
/*
 之前在初始化过程中讨论过 vfs 对应的索引节点，其实索引节点主要是指存在磁盘中的索引节点，
 当把磁盘中的索引节点 load 到内存中之后，在内存中也会存在一个索引节点
 
 对于磁盘索引节点，direct 指的是这个 inode 的直接索引块的索引值，它的大小是 12，所以最多能够
 通过 direct 的方式支持最大 12 * 4k 的文件大小。之所以这样设计是因为我们实际的文件系统中，
 绝大多数文件都是小文件，因此直接索引的方式能够提高小文件的存取速度
 
 如果要支持大文件存储，就要通过间接索引的方式。当使用一级间接数据块索引时，ucore 支持最大的文件
 大小为 12 * 4k + 1024 * 4k = 48k + 4m。
*/
struct sfs_disk_inode
{
    // 如果 inode 表示常规文件，则 size 是文件总的大小
    uint32_t size;                                  /* size of the file (in bytes) */
    // inode 的文件类型
    uint16_t type;                                  /* one of SYS_TYPE_* above */
    // 此 inode 的硬链接数
    uint16_t nlinks;                                /* # of hard links to this file */
    // 此 inode 拥有的数据块的个数(direct + indirect)，如果是个目录 node，
    // 这个也表示目录下文件的个数
    uint32_t blocks;                                /* # of blocks */
    // 此 inode 的直接数据块索引值（有 SFS_NDIRECT 个)，这些索引对应的磁盘区域
    // 才是文件内容真正存放的地方
    uint32_t direct[SFS_NDIRECT];                   /* direct blocks */
    // direct 只能存放 12 个索引，放不下的用 indirect 间接索引
    uint32_t indirect;                              /* indirect blocks */
};

/* file entry (on disk 磁盘上的 entry 二进制结构) */
/*
 对于普通文件，索引值指向的 block 中保存的是文件中的数据。而对于目录，索引值指向的数据保存的
 是目录下所有的文件名以及对应的索引节点所在的索引块（磁盘块）所形成的数组
 
 操作系统中，每个文件系统下的 inode 都应该分配唯一的 inode 编号。SFS 下，为了实现的简便，
 每个 inode 直接用他所在的磁盘 block 的编号作为 inode 编号。
 比如，root block 的 inode 编号为 1；每个 sfs_disk_entry 数据结构中，name 表示目录下
 文件或文件夹的名称，ino 表示磁盘 block 编号，通过读取该 block 的数据，能够得到相应的文件
 或文件夹的 inode。ino 为 0 时，表示一个无效的 entry。（因为 block 0 用来保存 super block，
 它不可能被其他任何文件或目录使用，所以这么设计也是合理的）。
 此外，和 inode 相似，每个 sfs_dirent_entry 也占用一个 block。
*/
struct sfs_disk_entry
{
    // 索引节点所占数据块索引值
    uint32_t ino;                                   /* inode number */
    // 文件名
    char name[SFS_MAX_FNAME_LEN + 1];               /* file name */
};

#define sfs_dentry_size                             \
    sizeof(((struct sfs_disk_entry *)0)->name)

/* inode for sfs 内存中的 inode 结构 */
/*
 内存索引节点
 内存 inode 只有在打开一个文件后才会创建，如果关机则相关信息都会消失。
 可以看到，内存 inode 包含了硬盘 inode 的信息，而且还增加了其他一些信息，这是为了实现判断
 是否改写（dirty），互斥操作（sem），回收（reclaim —— count）和快速定位（hash_link）等作用。
*/
struct sfs_inode
{
    // 磁盘上存放的二进制数据机构，通过这个 inode 完成对文件、目录的打开，读写，关闭等
    struct sfs_disk_inode *din;                     /* on-disk inode */
    // node 节点编号，实际也是 inode 所在磁盘上第几个 block 的索引
    uint32_t ino;                                   /* inode number */
    bool dirty;                                     /* true if inode modified */
    int reclaim_count;                              /* kill inode if it hits zero */
    semaphore_t sem;                                /* semaphore for din */
    list_entry_t inode_link;                        /* entry for linked-list in sfs_fs */
    list_entry_t hash_link;                         /* entry for hash linked-list in sfs_fs */
};

#define le2sin(le, member)                          \
    to_struct((le), struct sfs_inode, member)

/* filesystem for sfs */
/*
 sfs 文件系统相关结构
*/
struct sfs_fs
{
    // super 超级块，这里是个结构体，不是指针
    struct sfs_super super;                         /* on-disk superblock */
    struct device *dev;                             /* device mounted on */
    struct bitmap *freemap;                         /* blocks in use are mared 0 */
    bool super_dirty;                               /* true if super/freemap modified */
    // 文件读写缓冲区，大小 4k
    void *sfs_buffer;                               /* buffer for non-block aligned io */
    semaphore_t fs_sem;                             /* semaphore for fs */
    semaphore_t io_sem;                             /* semaphore for io */
    semaphore_t mutex_sem;                          /* semaphore for link/unlink and rename */
    list_entry_t inode_list;                        /* inode linked-list */
    list_entry_t *hash_list;                        /* inode hash linked-list */
};

/* hash for sfs */
#define SFS_HLIST_SHIFT                             10
#define SFS_HLIST_SIZE                              (1 << SFS_HLIST_SHIFT)
#define sin_hashfn(x)                               (hash32(x, SFS_HLIST_SHIFT))

/* size of freemap (in bits) */
#define sfs_freemap_bits(super)                     ROUNDUP((super)->blocks, SFS_BLKBITS)

/* size of freemap (in blocks) */
#define sfs_freemap_blocks(super)                   ROUNDUP_DIV((super)->blocks, SFS_BLKBITS)

struct fs;
struct inode;

void sfs_init(void);
int sfs_mount(const char *devname);

void lock_sfs_fs(struct sfs_fs *sfs);
void lock_sfs_io(struct sfs_fs *sfs);
void unlock_sfs_fs(struct sfs_fs *sfs);
void unlock_sfs_io(struct sfs_fs *sfs);

int sfs_rblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);
int sfs_wblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);
int sfs_rbuf(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset);
int sfs_wbuf(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset);
int sfs_sync_super(struct sfs_fs *sfs);
int sfs_sync_freemap(struct sfs_fs *sfs);
int sfs_clear_block(struct sfs_fs *sfs, uint32_t blkno, uint32_t nblks);

int sfs_load_inode(struct sfs_fs *sfs, struct inode **node_store, uint32_t ino, const char *name);

#endif /* !__KERN_FS_SFS_SFS_H__ */

