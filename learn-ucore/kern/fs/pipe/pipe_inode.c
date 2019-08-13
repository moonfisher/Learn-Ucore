#include "defs.h"
#include "string.h"
#include "slab.h"
#include "vfs.h"
#include "inode.h"
#include "pipe.h"
#include "pipe_state.h"
#include "iobuf.h"
#include "stat.h"
#include "unistd.h"
#include "error.h"
#include "assert.h"

int pipe_inode_open(struct inode *node, uint32_t open_flags)
{
	if (open_flags & (O_TRUNC | O_APPEND))
    {
		return -E_INVAL;
	}
  
    struct pipe_inode *pin = pipe_inode_vop_info(node);
	switch (open_flags & O_ACCMODE)
    {
        case O_RDONLY:
            return (pin->pin_type == PIN_RDONLY) ? 0 : -E_INVAL;
        case O_WRONLY:
            return (pin->pin_type == PIN_WRONLY) ? 0 : -E_INVAL;
        default:
            return -E_INVAL;
	}
}

int pipe_inode_close(struct inode *node)
{
	struct pipe_inode *pin = pipe_inode_vop_info(node);
	pipe_state_close(pin->state);
	return 0;
}

int pipe_inode_read(struct inode *node, struct iobuf *iob)
{
	struct pipe_inode *pin = pipe_inode_vop_info(node);
	if (pin->pin_type != PIN_RDONLY)
    {
		return -E_INVAL;
	}
    
	size_t ret;
	if ((ret = pipe_state_read(pin->state, iob->io_base, iob->io_resid)) != 0)
    {
		iobuf_skip(iob, ret);
	}
	return 0;
}

int pipe_inode_write(struct inode *node, struct iobuf *iob)
{
	struct pipe_inode *pin = pipe_inode_vop_info(node);
	if (pin->pin_type != PIN_WRONLY)
    {
		return -E_INVAL;
	}
    
	size_t ret;
	if ((ret = pipe_state_write(pin->state, iob->io_base, iob->io_resid)) != 0)
    {
		iobuf_skip(iob, ret);
	}
	return 0;
}

int pipe_inode_fstat(struct inode *node, struct stat *stat)
{
	int ret;
	memset(stat, 0, sizeof(struct stat));
    if ((ret = node->in_ops->vop_gettype(node, &(stat->st_mode))) != 0)
    {
		return ret;
	}
	struct pipe_inode *pin = pipe_inode_vop_info(node);
	stat->st_nlinks = 1;
	stat->st_blocks = 0;
	stat->st_size = pipe_state_size(pin->state, pin->pin_type == PIN_WRONLY);
	return 0;
}

int pipe_inode_namefile(struct inode *node, struct iobuf *iob)
{
	struct pipe_inode *pin = pipe_inode_vop_info(node);
	size_t len = (pin->name != NULL) ? strlen(pin->name) : 0;
	if (iob->io_resid < len + 1)
    {
		return -E_NO_MEM;
	}
	if (pin->name != NULL)
    {
		memcpy(iob->io_base, pin->name, len);
	}
	((char *)(iob->io_base))[len++] = '\0';
	iobuf_skip(iob, len);
	return 0;
}

int pipe_inode_reclaim(struct inode *node)
{
	struct pipe_inode *pin = pipe_inode_vop_info(node);
	if (pin->name != NULL)
    {
        assert(((node)->in_fs) != NULL && (((node)->in_fs)->fs_type == fs_type_pipe_info));
        struct pipe_fs *pipe = &(((node)->in_fs)->fs_info.__pipe_info);
		lock_pipe(pipe);
		assert(pin->reclaim_count > 0);
		if ((--pin->reclaim_count) != 0 || inode_ref_count(node) != 0)
        {
			unlock_pipe(pipe);
			return -E_BUSY;
		}
		list_del(&(pin->pipe_link));
		unlock_pipe(pipe);
		kfree(pin->name);
	}
	pipe_state_release(pin->state);
	inode_kill(node);
	return 0;
}

int pipe_inode_gettype(struct inode *node, uint32_t * type_store)
{
	*type_store = S_IFCHR;
	return 0;
}

const struct inode_ops pipe_node_ops =
{
	.vop_magic          = VOP_MAGIC,
	.vop_open           = pipe_inode_open,
	.vop_close          = pipe_inode_close,
	.vop_read           = pipe_inode_read,
	.vop_write          = pipe_inode_write,
	.vop_fstat          = pipe_inode_fstat,
	.vop_fsync          = (void *)null_vop_pass,
	.vop_mkdir          = (void *)null_vop_notdir,
	.vop_link           = (void *)null_vop_notdir,
	.vop_rename         = (void *)null_vop_notdir,
	.vop_readlink       = (void *)null_vop_inval,
	.vop_symlink        = (void *)null_vop_notdir,
	.vop_namefile       = pipe_inode_namefile,
	.vop_getdirentry    = (void *)null_vop_inval,
	.vop_reclaim        = pipe_inode_reclaim,
	.vop_ioctl          = (void *)null_vop_inval,
	.vop_gettype        = pipe_inode_gettype,
	.vop_tryseek        = (void *)null_vop_inval,
	.vop_truncate       = (void *)null_vop_inval,
	.vop_create         = (void *)null_vop_notdir,
	.vop_unlink         = (void *)null_vop_notdir,
	.vop_lookup         = (void *)null_vop_notdir,
	.vop_lookup_parent  = (void *)null_vop_notdir,
};

void pipe_inode_init(struct pipe_inode *pin, char *name, struct pipe_state *state, bool readonly)
{
	assert(state != NULL);
	pin->pin_type = readonly ? PIN_RDONLY : PIN_WRONLY;
    pin->name = name;
    pin->state = state;
    pin->reclaim_count = 1;
	list_init(&(pin->pipe_link));
}

struct inode *pipe_create_inode(struct fs *fs, const char *__name, struct pipe_state *state, bool readonly)
{
	char *name = NULL;
	if (__name == NULL || (name = strdup(__name)) != NULL)
    {
		struct inode *node;
		if ((node = __alloc_inode(inode_type_pipe_inode_info)) != NULL)
        {
            inode_init(node, &pipe_node_ops, fs);
			pipe_inode_init(pipe_inode_vop_info(node), name, state, readonly);
			return node;
		}
		if (name != NULL)
        {
			kfree(name);
		}
	}
	return NULL;
}

int pipe_open(struct inode **rnode_store, struct inode **wnode_store)
{
	int ret;
	struct inode *root;
	if ((ret = vfs_get_root("pipe", &root)) != 0)
    {
		return ret;
	}
	ret = -E_NO_MEM;

	struct pipe_state *state;
	if ((state = pipe_state_create()) == NULL)
    {
		goto out;
	}

    struct fs *fs = root->in_fs;
	struct inode *node[2] = { NULL, NULL };
	if ((node[0] = pipe_create_inode(fs, NULL, state, 1)) == NULL)
    {
		goto failed_cleanup_state;
	}

	pipe_state_acquire(state);
	if ((node[1] = pipe_create_inode(fs, NULL, state, 0)) == NULL)
    {
		goto failed_cleanup_node0;
	}

    inode_open_inc(node[0]);
    inode_open_inc(node[1]);

	*rnode_store = node[0];
	*wnode_store = node[1];
	ret = 0;

out:
	inode_ref_dec(root);
	return ret;

failed_cleanup_node0:
	inode_ref_dec(node[0]);
failed_cleanup_state:
	pipe_state_release(state);
	goto out;
}
