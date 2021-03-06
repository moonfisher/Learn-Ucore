#include "ulib.h"
#include "stdio.h"
#include "string.h"
#include "dir.h"
#include "../libs/file.h"
#include "error.h"
#include "unistd.h"
#include "stat.h"

#define printf(...)                     fprintf(1, __VA_ARGS__)
#define putc(c)                         printf("%c", c)

#define BUFSIZE                         4096
#define WHITESPACE                      " \t\r\n"
#define SYMBOLS                         "<|>&;"

// 这里定义的全局变量无法实现父子进程共享，参考写时复制
char shcwd[BUFSIZE];

int gettoken(char **p1, char **p2)
{
    char *s;
    if ((s = *p1) == NULL)
    {
        return 0;
    }
    while (strchr(WHITESPACE, *s) != NULL)
    {
        *s ++ = '\0';
    }
    if (*s == '\0')
    {
        return 0;
    }

    *p2 = s;
    int token = 'w';
    if (strchr(SYMBOLS, *s) != NULL)
    {
        token = *s;
        *s ++ = '\0';
    }
    else
    {
        bool flag = 0;
        while (*s != '\0' && (flag || strchr(WHITESPACE SYMBOLS, *s) == NULL))
        {
            if (*s == '"')
            {
                *s = ' ';
                flag = !flag;
            }
            s++;
        }
    }
    *p1 = (*s != '\0' ? s : NULL);
    return token;
}

char *readline(const char *prompt)
{
    static char buffer[BUFSIZE];
    if (prompt != NULL)
    {
        printf("%s", prompt);
    }
    
    int ret, i = 0;
    while (1)
    {
        char c;
        if ((ret = read(0, &c, sizeof(char))) < 0)
        {
            return NULL;
        }
        else if (ret == 0)
        {
            if (i > 0)
            {
                buffer[i] = '\0';
                break;
            }
            return NULL;
        }

        if (c == 3)
        {
            return NULL;
        }
        else if (c >= ' ' && i < BUFSIZE - 1)
        {
            putc(c);
            buffer[i ++] = c;
        }
        else if (c == '\b' && i > 0)
        {
            putc(c);
            i --;
        }
        else if (c == '\n' || c == '\r')
        {
            putc(c);
            buffer[i] = '\0';
            break;
        }
    }
    return buffer;
}

void usage(void)
{
    printf("usage: sh [command-file]\n");
}

int reopen(int fd2, const char *filename, uint32_t open_flags)
{
    int fd1 = 0;
    int ret = 0;
    fd1 = open(filename, open_flags);
    if (fd1 >= 0 && fd1 != fd2)
    {
        close(fd2);
        ret = dup2(fd1, fd2);
        close(fd1);
    }
    return ret < 0 ? ret : 0;
}

int testfile(const char *name)
{
    int ret = -1;
    int fd = 0;
    if ((fd = open(name, O_RDONLY)) < 0)
    {
        return fd;
    }
    
    struct stat __stat, *stat = &__stat;
    if ((ret = fstat(fd, stat)) != 0)
    {
        close(fd);
        return ret;
    }
    
    // 如果是目录不执行
    if (S_ISDIR(stat->st_mode))
    {
        close(fd);
        return ret;
    }
    
    close(fd);
    return 0;
}

int runcmd(char *cmd)
{
    static char argv0[BUFSIZE] = {0};
    const char *argv[EXEC_MAX_ARG_NUM + 1] = {0};
    char *t = NULL;
    int argc = 0, token = 0, ret = 0, p[2] = {0};
again:
    argc = 0;
    while (1)
    {
        switch (token = gettoken(&cmd, &t))
        {
            case 'w':
                if (argc == EXEC_MAX_ARG_NUM)
                {
                    printf("sh error: too many arguments\n");
                    return -1;
                }
                argv[argc ++] = t;
                break;
            case '<':
                if (gettoken(&cmd, &t) != 'w')
                {
                    printf("sh error: syntax error: < not followed by word\n");
                    return -1;
                }
                if ((ret = reopen(0, t, O_RDONLY)) != 0)
                {
                    return ret;
                }
                break;
            case '>':
                if (gettoken(&cmd, &t) != 'w')
                {
                    printf("sh error: syntax error: > not followed by word\n");
                    return -1;
                }
                if ((ret = reopen(1, t, O_RDWR | O_TRUNC | O_CREAT)) != 0)
                {
                    return ret;
                }
                break;
            case '|':
              //  if ((ret = pipe(p)) != 0) {
              //      return ret;
              //  }
                if ((ret = fork("pipe")) == 0)
                {
                    close(0);
                    if ((ret = dup2(p[0], 0)) < 0)
                    {
                        return ret;
                    }
                    close(p[0]);
                    close(p[1]);
                    goto again;
                }
                else
                {
                    if (ret < 0)
                    {
                        return ret;
                    }
                    close(1);
                    if ((ret = dup2(p[1], 1)) < 0)
                    {
                        return ret;
                    }
                    close(p[0]);
                    close(p[1]);
                    goto runit;
                }
                break;
            case 0:
                goto runit;
            case ';':
                if ((ret = fork(";")) == 0)
                {
                    goto runit;
                }
                else
                {
                    if (ret < 0)
                    {
                        return ret;
                    }
                    waitpid(ret, NULL);
                    goto again;
                }
                break;
            default:
                printf("sh error: bad return %d from gettoken\n", token);
                return -1;
        }
    }

runit:
    if (argc == 0)
    {
        return 0;
    }
    // cd xxx，需要切换的是 sh 所在的文件目录，这里不能用子进程 cd 去切换
    // 子进程 cd 切换的只是 cd 当前的目录结构
    else if (strcmp(argv[0], "cd") == 0)
    {
        if (argc != 2)
        {
            return -1;
        }
        // 当 shcwd 变量不修改时，子进程和父进程看到的是同一个变量（虚拟地址空间），但当子进程修改时，
        // 会采用写时复制，子进程和父进程的 shcwd 在内存物理地址上不是同一个地方，相互不影响
        strcpy(shcwd, argv[1]);
        return 0;
    }
    
    if ((ret = testfile(argv[0])) != 0)
    {
        if (ret != -E_NOENT)
        {
            return ret;
        }
        // 如果找不到文件，尝试从根目录下再次查找一次，这是为了解决进入子目录之后
        // 根目录下的命令无法执行的问题，此时还没有环境变量的概念
        snprintf(argv0, sizeof(argv0), "/%s", argv[0]);
        argv[0] = argv0;
    }
    
    argv[argc] = NULL;
    return __exec(argv[0], argv);
}

int main(int argc, char **argv)
{
    printf("user sh is running!!!\n");
    int ret, interactive = 1;
    if (argc == 2)
    {
        if ((ret = reopen(0, argv[1], O_RDONLY)) != 0)
        {
            return ret;
        }
        interactive = 0;
    }
    else if (argc > 2)
    {
        usage();
        return -1;
    }
    //shcwd = malloc(BUFSIZE);
    assert(shcwd != NULL);

    char *buffer;
    while ((buffer = readline((interactive) ? "$ " : NULL)) != NULL)
    {
        char cmd[1024] = {0};
        strncpy(cmd, buffer, strlen(buffer));
        
        char *p = strtok(cmd, " ");
        char *q;
        // cd xxx，需要切换的是 sh 所在的文件目录，这里不能用子进程 cd 去切换
        // 子进程 cd 切换的只是子进程当前的目录结构，sh 不影响
        if (p && (strcmp(p, "cd") == 0))
        {
            q = strtok(NULL, " ");
            if (q && (strlen(q) > 0))
            {
                ret = chdir(q);
                if (ret != 0)
                {
                    printf("change dir: [%s] not exist.\n", q);
                }
                else
                {
                    printf("change dir: [%s] success.\n", q);
                }
            }
        }
        else
        {
            shcwd[0] = '\0';
            int pid = 0;
            char *name = "runcmd";
            if (p)
            {
                name = p;
            }
            
            if ((pid = fork(name)) == 0)
            {
                ret = runcmd(buffer);
                exit(ret);
            }
            
            assert(pid >= 0);
            if (waitpid(pid, &ret) == 0)
            {
                // 因为有写时复制，这里 shcwd 还是之前的值，子进程无法修改 shcwd
                if (ret == 0 && shcwd[0] != '\0')
                {
                    ret = chdir(shcwd);
                }
                if (ret != 0)
                {
                    printf("error: %d - %e\n", ret, ret);
                }
            }
        }
    }
    
    return 0;
}

