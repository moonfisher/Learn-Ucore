/* prefer to compile mksfs on 64-bit linux systems.

Use a compiler-specific macro.

For example:

#if defined(__i386__)
// IA-32
#elif defined(__x86_64__)
// AMD64
#else
# error Unsupported architecture
#endif

*/

#if 1

#include "defs.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "dirent.h"
#include "unistd.h"
#include "stat.h"
#include "malloc.h"
#include "dir.h"
#include "ulib.h"
#include "file.h"

/* 2^31 + 2^29 - 2^25 + 2^22 - 2^19 - 2^16 + 1 */
#define GOLDEN_RATIO_PRIME_32       0x9e370001UL

#define HASH_SHIFT                  10
#define HASH_LIST_SIZE              (1 << HASH_SHIFT)

void *safe_malloc(size_t size)
{
    void *ret;
    if ((ret = malloc(size)) == NULL)
    {
        cprintf("malloc %lu bytes failed.\n", (long unsigned)size);
    }
    return ret;
}

struct stat *safe_fstat(int fd)
{
    static struct stat __stat;
    if (fstat(fd, &__stat) != 0)
    {
        cprintf("fstat %d failed.\n", fd);
        return NULL;
    }
    return &__stat;
}

void safe_fchdir(int fd)
{
    if (fchdir(fd) != 0)
    {
        cprintf("fchdir failed %d.\n", fd);
    }
}

#define SFS_MAGIC                               0x2f8dbe2a
#define SFS_NDIRECT                             12
#define SFS_BLKSIZE                             4096                                    // 4K
#define SFS_MAX_NBLKS                           (1024UL * 512)                          // 4K * 512K
#define SFS_MAX_INFO_LEN                        31
#define SFS_MAX_FNAME_LEN                       255
#define SFS_MAX_FILE_SIZE                       (1024UL * 1024 * 128)                   // 128M

#define SFS_BLKBITS                             (SFS_BLKSIZE * CHAR_BIT)
#define SFS_TYPE_FILE                           1
#define SFS_TYPE_DIR                            2
#define SFS_TYPE_LINK                           3

#define SFS_BLKN_SUPER                          0
#define SFS_BLKN_ROOT                           1
#define SFS_BLKN_FREEMAP                        2

struct cache_block
{
    uint32_t ino;
    struct cache_block *hash_next;
    void *cache;
};

struct cache_inode
{
    struct inode
    {
        uint32_t size;
        uint32_t slots;
        uint32_t parent;                                
        uint16_t type;
        uint16_t nlinks;
        uint32_t blocks;
        uint32_t direct[SFS_NDIRECT];
        uint32_t indirect;
        uint32_t db_indirect;
    } inode;
    uint64_t real;
    uint32_t ino;
    uint32_t nblks;
    struct cache_block *l1, *l2;
    struct cache_inode *hash_next;
};

struct sfs_fs
{
    struct
    {
        uint32_t magic;
        uint32_t blocks;
        uint32_t unused_blocks;
        char info[SFS_MAX_INFO_LEN + 1];
    } super;
    struct subpath
    {
        struct subpath *next;
        struct subpath *prev;
        char *subname;
    } __sp_nil, *sp_root, *sp_end;
    int imgfd;
    uint32_t ninos;
    uint32_t next_ino;
    struct cache_inode *root;
    struct cache_inode *inodes[HASH_LIST_SIZE];
    struct cache_block *blocks[HASH_LIST_SIZE];
};

struct sfs_entry
{
    uint32_t ino;
    char name[SFS_MAX_FNAME_LEN + 1];
};

static uint32_t sfs_alloc_ino(struct sfs_fs *sfs)
{
    if (sfs->next_ino < sfs->ninos)
    {
        sfs->super.unused_blocks --;
        return sfs->next_ino ++;
    }
    cprintf("out of disk space.\n");
    return 0;
}

static struct cache_block *alloc_cache_block(struct sfs_fs *sfs, uint32_t ino)
{
    struct cache_block *cb = safe_malloc(sizeof(struct cache_block));
    cb->ino = (ino != 0) ? ino : sfs_alloc_ino(sfs);
    cb->cache = memset(safe_malloc(SFS_BLKSIZE), 0, SFS_BLKSIZE);
    struct cache_block **head = sfs->blocks + hash32(ino, HASH_SHIFT);
    cb->hash_next = *head;
    *head = cb;
    return cb;
}

struct cache_block *search_cache_block(struct sfs_fs *sfs, uint32_t ino)
{
    struct cache_block *cb = sfs->blocks[hash32(ino, HASH_SHIFT)];
    while (cb != NULL && cb->ino != ino)
    {
        cb = cb->hash_next;
    }
    return cb;
}

static struct cache_inode *alloc_cache_inode(struct sfs_fs *sfs, uint64_t real, uint32_t ino, uint16_t type)
{
    struct cache_inode *ci = safe_malloc(sizeof(struct cache_inode));
    ci->ino = (ino != 0) ? ino : sfs_alloc_ino(sfs);
    ci->real = real;
    ci->nblks = 0;
    ci->l1 = ci->l2 = NULL;
    struct inode *inode = &(ci->inode);
    memset(inode, 0, sizeof(struct inode));
    inode->type = type;
    struct cache_inode **head = sfs->inodes + hash32((uint32_t)real, HASH_SHIFT);
    ci->hash_next = *head;
    *head = ci;
    return ci;
}

struct cache_inode *search_cache_inode(struct sfs_fs *sfs, uint64_t real)
{
    struct cache_inode *ci = sfs->inodes[hash32((uint32_t)real, HASH_SHIFT)];
    while (ci != NULL && ci->real != real)
    {
        ci = ci->hash_next;
    }
    return ci;
}

static void init_dir_cache_inode(struct cache_inode *current, struct cache_inode *parent)
{
    struct inode *inode = &(current->inode);
    assert(inode->type == SFS_TYPE_DIR && parent->inode.type == SFS_TYPE_DIR);
    assert(inode->nlinks == 0 && inode->slots == 0 && inode->parent == 0);
    inode->nlinks++;
    parent->inode.nlinks++;
    inode->parent = parent->ino;
}

struct sfs_fs *create_sfs(int imgfd)
{
    uint32_t ninos, next_ino;
    struct stat *stat = safe_fstat(imgfd);
    if ((ninos = stat->st_size / SFS_BLKSIZE) > SFS_MAX_NBLKS)
    {
        ninos = SFS_MAX_NBLKS;
        cprintf("img file is too big (%llu bytes, only use %u blocks).\n", (unsigned long long)stat->st_size, ninos);
        return NULL;
    }
    
    if ((next_ino = SFS_BLKN_FREEMAP + (ninos + SFS_BLKBITS - 1) / SFS_BLKBITS) >= ninos)
    {
        cprintf("img file is too small (%llu bytes, %u blocks, bitmap use at least %u blocks).\n", (unsigned long long)stat->st_size, ninos, next_ino - 2);
        return NULL;
    }

    struct sfs_fs *sfs = safe_malloc(sizeof(struct sfs_fs));
    sfs->super.magic = SFS_MAGIC;
    sfs->super.blocks = ninos;
    sfs->super.unused_blocks = ninos - next_ino;
    snprintf(sfs->super.info, SFS_MAX_INFO_LEN, "simple file system");

    sfs->ninos = ninos;
    sfs->next_ino = next_ino;
    sfs->imgfd = imgfd;
    sfs->sp_root = sfs->sp_end = &(sfs->__sp_nil);
    sfs->sp_end->prev = sfs->sp_end->next = NULL;

    int i;
    for (i = 0; i < HASH_LIST_SIZE; i ++)
    {
        sfs->inodes[i] = NULL;
        sfs->blocks[i] = NULL;
    }

    sfs->root = alloc_cache_inode(sfs, 0, SFS_BLKN_ROOT, SFS_TYPE_DIR);
    init_dir_cache_inode(sfs->root, sfs->root);
    return sfs;
}

static void subpath_push(struct sfs_fs *sfs, const char *subname)
{
    struct subpath *subpath = safe_malloc(sizeof(struct subpath));
    size_t len = strlen(subname);
    char *name = safe_malloc(len + 1);
    memcpy(name, subname, len);
    name[len] = '\0';
    subpath->subname = name;
    sfs->sp_end->next = subpath;
    subpath->prev = sfs->sp_end;
    subpath->next = NULL;
    sfs->sp_end = subpath;
}

static void subpath_pop(struct sfs_fs *sfs)
{
    assert(sfs->sp_root != sfs->sp_end);
    struct subpath *subpath = sfs->sp_end;
    sfs->sp_end = sfs->sp_end->prev;
    sfs->sp_end->next = NULL;
    free(subpath->subname);
    free(subpath);
}

static void write_block(struct sfs_fs *sfs, void *data, size_t len, uint32_t ino)
{
    assert(len <= SFS_BLKSIZE && ino < sfs->ninos);
    static char buffer[SFS_BLKSIZE];
    if (len != SFS_BLKSIZE)
    {
        memset(buffer, 0, sizeof(buffer));
        data = memcpy(buffer, data, len);
    }
    off_t offset = (off_t)ino * SFS_BLKSIZE;
    ssize_t ret;
    ret = seek(sfs->imgfd, offset, LSEEK_SET);
    if ((ret = write(sfs->imgfd, data, SFS_BLKSIZE)) != SFS_BLKSIZE)
    {
        cprintf("write %u block failed: (%d/%d).\n", ino, (int)ret, SFS_BLKSIZE);
    }
}

static void flush_cache_block(struct sfs_fs *sfs, struct cache_block *cb)
{
    write_block(sfs, cb->cache, SFS_BLKSIZE, cb->ino);
}

static void flush_cache_inode(struct sfs_fs *sfs, struct cache_inode *ci)
{
    write_block(sfs, &(ci->inode), sizeof(ci->inode), ci->ino);
}

void close_sfs(struct sfs_fs *sfs)
{
    static char buffer[SFS_BLKSIZE];
    uint32_t i, j, ino = SFS_BLKN_FREEMAP;
    uint32_t ninos = sfs->ninos, next_ino = sfs->next_ino;
    for (i = 0; i < ninos; ino ++, i += SFS_BLKBITS)
    {
        memset(buffer, 0, sizeof(buffer));
        if (i + SFS_BLKBITS > next_ino)
        {
            uint32_t start = 0, end = SFS_BLKBITS;
            if (i < next_ino)
            {
                start = next_ino - i;
            }
            if (i + SFS_BLKBITS > ninos)
            {
                end = ninos - i;
            }
            uint32_t *data = (uint32_t *)buffer;
            const uint32_t bits = sizeof(bits) * CHAR_BIT;
            for (j = start; j < end; j ++)
            {
                data[j / bits] |= (1 << (j % bits));
            }
        }
        write_block(sfs, buffer, sizeof(buffer), ino);
    }
    write_block(sfs, &(sfs->super), sizeof(sfs->super), SFS_BLKN_SUPER);

    for (i = 0; i < HASH_LIST_SIZE; i ++)
    {
        struct cache_block *cb = sfs->blocks[i];
        while (cb != NULL)
        {
            flush_cache_block(sfs, cb);
            cb = cb->hash_next;
        }
        struct cache_inode *ci = sfs->inodes[i];
        while (ci != NULL)
        {
            flush_cache_inode(sfs, ci);
            ci = ci->hash_next;
        }
    }
}

int open_img(const char *imgname)
{
    const char *expect = ".img";
    const char *ext = imgname + strlen(imgname) - strlen(expect);
    if (ext <= imgname || strcmp(ext, expect) != 0)
    {
        cprintf("invalid .img file name '%s'.\n", imgname);
        return -1;
    }
    
    int imgfd;
    if ((imgfd = open(imgname, O_WRONLY)) < 0)
    {
        cprintf("open '%s' failed.\n", imgname);
        return -1;
    }
    
    return imgfd;
}

void open_dir(struct sfs_fs *sfs, struct cache_inode *current, struct cache_inode *parent);
void open_file(struct sfs_fs *sfs, struct cache_inode *file, const char *filename, int fd);
void open_link(struct sfs_fs *sfs, struct cache_inode *file, const char *filename);

#define SFS_BLK_NENTRY                          (SFS_BLKSIZE / sizeof(uint32_t))
#define SFS_L0_NBLKS                            SFS_NDIRECT
#define SFS_L1_NBLKS                            (SFS_BLK_NENTRY + SFS_L0_NBLKS)
#define SFS_L2_NBLKS                            (SFS_BLK_NENTRY * SFS_BLK_NENTRY + SFS_L1_NBLKS)
#define SFS_LN_NBLKS                            (SFS_MAX_FILE_SIZE / SFS_BLKSIZE)

static void update_cache(struct sfs_fs *sfs, struct cache_block **cbp, uint32_t *inop)
{
    uint32_t ino = *inop;
    struct cache_block *cb = *cbp;
    if (ino == 0)
    {
        cb = alloc_cache_block(sfs, 0);
        ino = cb->ino;
    }
    else if (cb == NULL || cb->ino != ino)
    {
        cb = search_cache_block(sfs, ino);
        assert(cb != NULL && cb->ino == ino);
    }
    *cbp = cb;
    *inop = ino;
}

static void append_block(struct sfs_fs *sfs, struct cache_inode *file, size_t size, uint32_t ino, const char *filename)
{
    assert(size <= SFS_BLKSIZE);
    uint32_t nblks = file->nblks;
    struct inode *inode = &(file->inode);
    if (nblks >= SFS_LN_NBLKS)
    {
        cprintf("file %s is too big.\n", filename);
        return;
    }
    
    if (nblks < SFS_L0_NBLKS)
    {
        inode->direct[nblks] = ino;
    }
    else if (nblks < SFS_L1_NBLKS)
    {
        nblks -= SFS_L0_NBLKS;
        update_cache(sfs, &(file->l1), &(inode->indirect));
        uint32_t *data = file->l1->cache;
        data[nblks] = ino;
    }
    else if (nblks < SFS_L2_NBLKS)
    {
        nblks -= SFS_L1_NBLKS;
        update_cache(sfs, &(file->l2), &(inode->db_indirect));
        uint32_t *data2 = file->l2->cache;
        update_cache(sfs, &(file->l1), &data2[nblks / SFS_BLK_NENTRY]);
        uint32_t *data1 = file->l1->cache;
        data1[nblks % SFS_BLK_NENTRY] = ino;
    }
    file->nblks ++;
    inode->size += size;
    inode->blocks ++;
}

static void add_entry(struct sfs_fs *sfs, struct cache_inode *current, struct cache_inode *file, const char *name)
{
    static struct sfs_entry __entry, *entry = &__entry;
    assert(current->inode.type == SFS_TYPE_DIR && strlen(name) <= SFS_MAX_FNAME_LEN);
    entry->ino = file->ino;
    strcpy(entry->name, name);
    uint32_t entry_ino = sfs_alloc_ino(sfs);
    write_block(sfs, entry, sizeof(entry->name), entry_ino);
    current->inode.slots++;
    append_block(sfs, current, sizeof(entry->name), entry_ino, name);
    file->inode.nlinks ++;
}

static void add_dir(struct sfs_fs *sfs, struct cache_inode *parent, const char *dirname, int curfd, int fd, uint64_t real)
{
    assert(search_cache_inode(sfs, real) == NULL);
    struct cache_inode *current = alloc_cache_inode(sfs, real, 0, SFS_TYPE_DIR);
    init_dir_cache_inode(current, parent);
    safe_fchdir(fd);
    subpath_push(sfs, dirname);
    open_dir(sfs, current, parent);
    safe_fchdir(curfd);
    subpath_pop(sfs);
    add_entry(sfs, parent, current, dirname);
}

static void add_file(struct sfs_fs *sfs, struct cache_inode *current, const char *filename, int fd, uint64_t real)
{
    struct cache_inode *file;
    if ((file = search_cache_inode(sfs, real)) == NULL)
    {
        file = alloc_cache_inode(sfs, real, 0, SFS_TYPE_FILE);
        open_file(sfs, file, filename, fd);
    }
    add_entry(sfs, current, file, filename);
}

static void add_link(struct sfs_fs *sfs, struct cache_inode *current, const char *filename, uint64_t real)
{
    struct cache_inode *file = alloc_cache_inode(sfs, real, 0, SFS_TYPE_LINK);
    open_link(sfs, file, filename);
    add_entry(sfs, current, file, filename);
}

void open_dir(struct sfs_fs *sfs, struct cache_inode *current, struct cache_inode *parent)
{
    DIR *dir;
    if ((dir = opendir(".")) == NULL)
    {
        cprintf("opendir failed.\n");
        return;
    }

    struct dirent *direntp;
    while ((direntp = readdir(dir)) != NULL)
    {
        const char *name = direntp->name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        {
            continue ;
        }
        if (name[0] == '.')
        {
            continue ;
        }
        if (strlen(name) > SFS_MAX_FNAME_LEN)
        {
            cprintf("file name is too long: %s\n", name);
            continue;
        }
        
        int fd;
        if ((fd = open(name, O_RDONLY)) < 0)
        {
            cprintf("open failed: %s\n", name);
            continue;
        }
        
        struct stat *stat = safe_fstat(fd);
        if (S_ISLNK(stat->st_mode))
        {
            add_link(sfs, current, name, stat->st_ino);
        }
        else
        {
            if (S_ISDIR(stat->st_mode))
            {
                add_dir(sfs, current, name, dir->fd, fd, stat->st_ino);
            }
            else if (S_ISREG(stat->st_mode))
            {
                add_file(sfs, current, name, fd, stat->st_ino);
            }
            else
            {
                char mode = '?';
                if (S_ISFIFO(stat->st_mode)) mode = 'f';
                if (S_ISSOCK(stat->st_mode)) mode = 's';
                if (S_ISCHR(stat->st_mode)) mode = 'c';
                if (S_ISBLK(stat->st_mode)) mode = 'b';
                cprintf("unsupported mode %07x (%c): file %s\n", stat->st_mode, mode, name);
            }
            close(fd);
        }
    }
    closedir(dir);
}

void open_file(struct sfs_fs *sfs, struct cache_inode *file, const char *filename, int fd)
{
    static char buffer[SFS_BLKSIZE];
    ssize_t ret, last = SFS_BLKSIZE;
    while ((ret = read(fd, buffer, sizeof(buffer))) != 0)
    {
        assert(last == SFS_BLKSIZE);
        uint32_t ino = sfs_alloc_ino(sfs);
        write_block(sfs, buffer, ret, ino);
        append_block(sfs, file, ret, ino, filename);
        last = ret;
    }
    if (ret < 0)
    {
        cprintf("read file %s failed.\n", filename);
    }
}

void open_link(struct sfs_fs *sfs, struct cache_inode *file, const char *filename)
{
    static char buffer[SFS_BLKSIZE];
    uint32_t ino = sfs_alloc_ino(sfs);
    ssize_t ret = 0;//readlink(filename, buffer, sizeof(buffer));
    if (ret < 0 || ret == SFS_BLKSIZE)
    {
        cprintf("read link %s failed, %d", filename, (int)ret);
    }
    write_block(sfs, buffer, ret, ino);
    append_block(sfs, file, ret, ino, filename);
}

int create_img(struct sfs_fs *sfs, const char *home)
{
    int curfd, homefd;
    if ((curfd = open(".", O_RDONLY)) < 0)
    {
        cprintf("get current fd failed.\n");
        return -1;
    }
    
    if ((homefd = open(home, O_RDONLY)) < 0)
    {
        cprintf("open home directory '%s' failed.\n", home);
        return -1;
    }
    
    safe_fchdir(homefd);
    open_dir(sfs, sfs->root, sfs->root);
    safe_fchdir(curfd);
    close(curfd);
    close(homefd);
    close_sfs(sfs);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        cprintf("usage: <input *.img> <input dirname>\n");
        return -1;
    }
    
    const char *imgname = argv[1];
    const char *home = argv[2];
    
    int imgfd = open_img(imgname);
    struct sfs_fs *sfs = create_sfs(imgfd);
    
    if (create_img(sfs, home) != 0)
    {
        cprintf("create img failed.\n");
        return -1;
    }
    
    cprintf("create %s (%s) successfully.\n", imgname, home);
    return 0;
}

#else

int main(int argc, char **argv)
{
    return 0;
}

#endif


