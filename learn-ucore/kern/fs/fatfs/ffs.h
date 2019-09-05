#ifndef __KERN_FS_FATFS_FFS_H__
#define __KERN_FS_FATFS_FFS_H__

#include "defs.h"
#include "mmu.h"
#include "list.h"
#include "sem.h"
#include "unistd.h"
#include "fatfs/ff.h"

#define FFS_MAGIC			0x19ad6b2f	            /* magic number for ffs */
#define FFS_BLKSIZE			PGSIZE                  /* size of block */
#define FFS_NDIRECT			12	                    /* # of direct blocks in inode */
#define FFS_MAX_INFO_LEN	31	                    /* max length of information */
#define FFS_MAX_FNAME_LEN   FS_MAX_FNAME_LEN	    /* max length of filename */
#define FFS_MAX_FILE_SIZE   (1024UL * 1024 * 64)	/* max file size (64M) */
#define FFS_BLKN_SUPER      0	                    /* block the superblock lives in */
#define FFS_BLKN_ROOT       1	                    /* location of the root dir inode */
#define FFS_BLKN_FREEMAP    2	                    /* 1st block of the freemap */

/* # of bits in a block */
#define FFS_BLKBITS                                 (FFS_BLKSIZE * CHAR_BIT)

/* # of entries in a block */
#define FFS_BLK_NENTRY                              (FFS_BLKSIZE / sizeof(uint32_t))

/* file types */
#define FFS_TYPE_INVAL                              0	/* Should not appear on disk */
#define FFS_TYPE_FILE                               1
#define FFS_TYPE_DIR                                2
#define FFS_TYPE_LINK                               3

/*
 如何制作 fat.img
 在 Linux 上可以通过如下办法制作一个 2M 的 fat32 格式的分区
 dd if=/dev/zero of=fat.img count=4000
 mkfs -t vfat -F 32 fat.img
 然后挂载 sudo mount -o loop -t vfat fat.img /mnt
 这样通过 /mnt 直接往里面拷贝文件
*/

/*
 * On-disk superblock
 * no use in fat32
 */
/*
 FAT32 是没有 superblock 的，虽然这个文件中也进行了定义，但实际上没有任何地方使用到它。
 然后是 ffs inode，因为 FAT32 原来是没有 inode 的，所以就需要人为的对它进行定义和赋值。
 这里去掉了原有的 ino，即 inode number。然后为了识别每一个特定的 inode，在 inode 中添加
 了绝对路径的信息和父目录的 inode 的信息。
 
 其他 sfs 类似，在 ffs disk inode 中，因为已经不需要记录文件的位置等信息（因为有 FatFs 库）
 所以我们只需要记录此 inode 对应的文件或者文件夹的内存的指针即可。
*/
struct ffs_super
{
	uint32_t magic;		                /* magic number, should be FFS_MAGIC */
	uint32_t blocks;	                /* # of blocks in fs */
	uint32_t unused_blocks;	            /* # of unused blocks in fs */
	char info[FFS_MAX_INFO_LEN + 1];	/* infomation for ffs  */
};

/* inode (on disk) */
/*
 在 sfs disk inode 中，会记录文件或文件夹在磁盘中的位置，但是这里有 FatFs 库了，
 所以无需那部分信息，只需要记录 FatFs 库需要的信息就可以了。
 在 ffs inode 中已经记录了绝对路径，这里需要 FIL 或者 DIR，即这个 inode 对应的文件或者文件夹的信息
*/
struct ffs_disk_inode
{
	uint16_t type;		                /* one of SYS_TYPE_* above */
	FILINFO info;
	uint32_t size;
	uint32_t blocks;
	union
    {
		struct FIL *file;
		struct DIR *dir;
	} entity;
};

/* file entry (on disk) */
struct ffs_disk_entry
{
	uint32_t ino;		                /* inode number */
	char name[FFS_MAX_FNAME_LEN + 1];	/* file name */
};

#define ffs_dentry_size         sizeof(((struct ffs_disk_entry *)0)->name)

/* inode for ffs */
/*
 由于 FAT32 的 inode 不同于 sfs 的 inode，所以要维护好自己特有的一些信息。
 ffs inode 以及 ffs disk inode 中不同于原有文件系统的部分。
 由于 FAT32 是没有 inode 的，所以就需要我们自己构建 inode。
 
 在 sfs inode 中，会有一个 ino，即 inode number，来唯一标识每一个 sfs inode
 FatFs 库很多函数都是直接需要绝对路径，而 ffs inode 又是自己构建的，所以直接用绝对路
 径来代替原来的 ino
*/
struct ffs_inode
{
	struct ffs_disk_inode *din;	        /* on-disk inode */
    // 为了遍历时加快速度，通过 path 计算 hashno
	uint32_t hashno;	                /* hash number */
	TCHAR *path;		                /* absolute path */
    // parent 是指向该文件或文件夹所在的父目录的 ffs inode，这是为了在查询的时候方便
	struct ffs_inode *parent;	        /* parent inode */
	bool dirty;		                    /* true if inode modified */
	int reclaim_count;	                /* kill inode if it hits zero */
	semaphore_t sem;	                /* semaphore for din */
//    list_entry_t inode_link;            /* entry for linked-list in ffs_fs */
//    list_entry_t hash_link;             /* entry for hash linked-list in ffs_fs */
    // 保存所有已经创建的 inode 的一个链表
	struct ffs_inode_list *inode_link;	/* entry for linked-list in ffs_fs */
};

struct ffs_inode_list
{
	struct ffs_inode *f_inode;
	struct ffs_inode_list *prev;
	struct ffs_inode_list *next;
};

#define le2fin(le, member)  to_struct((le), struct ffs_inode, member)

/* filesystem for ffs */
struct ffs_fs
{
	struct ffs_super super;
	struct device *dev;
	bool super_dirty;
	struct ffs_inode_list *inode_list;
	uint32_t inocnt;
	FATFS *fatfs;
};

#define FFS_HLIST_SHIFT	    10
#define FFS_HLIST_SIZE	    (1 << FFS_HLIST_SHIFT)

#define FFS_PATH            "/"

struct fs;
struct inode;

void ffs_init(void);
int ffs_mount(const char *devname);

void lock_ffs_fs(struct ffs_fs *ffs);
void lock_ffs_io(struct ffs_fs *ffs);
void lock_ffs_mutex(struct ffs_fs *ffs);
void unlock_ffs_fs(struct ffs_fs *ffs);
void unlock_ffs_io(struct ffs_fs *ffs);
void unlock_ffs_mutex(struct ffs_fs *ffs);

int ffs_rblock(struct ffs_fs *ffs, void *buf, uint32_t blkno, uint32_t nblks);
int ffs_wblock(struct ffs_fs *ffs, void *buf, uint32_t blkno, uint32_t nblks);
int ffs_rbuf(struct ffs_fs *ffs, void *buf, size_t len, uint32_t blkno, off_t offset);
int ffs_wbuf(struct ffs_fs *ffs, void *buf, size_t len, uint32_t blkno, off_t offset);
int ffs_sync_super(struct ffs_fs *ffs);
int ffs_sync_freemap(struct ffs_fs *ffs);
int ffs_clear_block(struct ffs_fs *ffs, uint32_t blkno, uint32_t nblks);
int ffs_load_inode(struct ffs_fs *ffs, struct inode **node_store, TCHAR * path, struct ffs_inode *parent, const char *name);

#endif //__KERN_FS_FATFS_FFS_H__
