#include "ulib.h"
#include "file.h"
#include "stat.h"
#include "unistd.h"

#define printf(...)                     fprintf(1, __VA_ARGS__)

int main(int argc, char **argv)
{
	if (argc == 2)
    {
        int ret = 0;
        if ((ret = open(argv[1], O_RDONLY)) >= 0)
        {
            printf("file %s exist.\n", argv[1]);
            return ret;
        }
        
        ret = open(argv[1], O_RDWR | O_CREAT);
        if (ret >= 0)
        {
            printf("create file %s success.\n", argv[1]);
        }
    }
    
	return 0;
}
