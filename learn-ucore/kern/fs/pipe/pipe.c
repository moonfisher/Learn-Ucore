#include "defs.h"
#include "slab.h"
#include "list.h"
#include "sem.h"
#include "vfs.h"
#include "inode.h"
#include "pipe.h"
#include "error.h"
#include "assert.h"

void lock_pipe(struct pipe_fs *pipe)
{
	down(&(pipe->pipe_sem));
}

void unlock_pipe(struct pipe_fs *pipe)
{
	up(&(pipe->pipe_sem));
}

int pipe_sync(struct fs *fs)
{
	return 0;
}

struct inode *pipe_get_root(struct fs *fs)
{
    struct pipe_fs *pipe = &(fs->fs_info.__pipe_info);
	inode_ref_inc(pipe->root);
	return pipe->root;
}

int pipe_unmount(struct fs *fs)
{
	return -E_INVAL;
}

void pipe_cleanup(struct fs *fs)
{
	/* do nothing */
    return;
}

void pipe_fs_init(struct fs *fs)
{
	struct pipe_fs *pipe = &(fs->fs_info.__pipe_info);
	if ((pipe->root = pipe_create_root(fs)) == NULL)
    {
		panic("pipe: create root inode failed.\n");
	}
	sem_init(&(pipe->pipe_sem), 1);
	list_init(&(pipe->pipe_list));

	fs->fs_sync = pipe_sync;
	fs->fs_get_root = pipe_get_root;
	fs->fs_unmount = pipe_unmount;
	fs->fs_cleanup = pipe_cleanup;
}

void pipe_init(void)
{
	struct fs *fs;
	if ((fs = __alloc_fs(fs_type_pipe_info)) == NULL)
    {
		panic("pipe: create pipe_fs failed.\n");
	}
	pipe_fs_init(fs);

	int ret;
	if ((ret = vfs_add_fs("pipe", fs)) != 0)
    {
		panic("pipe: vfs_add_fs: %e.\n", ret);
	}
}
