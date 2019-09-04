#include "defs.h"
#include "error.h"
#include "assert.h"
#include "fatfs/ffconf.h"
#include "ffs.h"
#include "vfs.h"
#include "stdio.h"

DWORD get_fattime(void)
{
	return ((DWORD) (2011 - 1980) << 25)
	    | ((DWORD) 3 << 21)
	    | ((DWORD) 26 << 16)
	    | ((DWORD) 19 << 11)
	    | ((DWORD) 28 << 5)
	    | ((DWORD) 0 << 1);
}

void ffs_init()
{
	int ret;
    if ((ret = register_filesystem("fatfs", ffs_mount)) != 0)
    {
        cprintf("failed: ffs: register_filesystem: %e.\n", ret);
        return;
    }
    
	if ((ret = ffs_mount("disk1")) != 0)
    {
		cprintf("failed: ffs: ffs_mount: %e.\n", ret);
	}
}

#if _FS_REENTRANT

bool ff_cre_syncobj(BYTE _vol, _SYNC_t * sobj)
{
	bool ret = false;
	return ret;
}

bool ff_del_syncobj(_SYNC_t sobj)
{
	bool ret = false;
	return ret;
}

bool ff_req_grant(_SYNC_t sobj)
{
	bool ret = false;
	return ret;
}

void ff_rel_grant(_SYNC_t sobj)

#endif

#if _USE_LFN == 3

void *ff_memalloc(UINT size)
{
	return malloc(size);
}

void ff_memfree(void *mblock)
{
	free(mblock);
}

#endif
