#include "ulib.h"
#include "stdio.h"
#include "dir.h"

#define printf(...)                     fprintf(1, __VA_ARGS__)

void usage(void)
{
	printf("usage: rm dir [...]\n");
}

int main(int argc, char **argv)
{
	if (argc != 2)
    {
		usage();
		return -1;
	}
    else
    {
        int ret = unlink(argv[1]);
        if (ret >= 0)
        {
            printf("rm %s success.\n", argv[1]);
            return ret;
        }
        
        printf("rm %s failed %e.\n", argv[1], ret);
    }
	return 0;
}
