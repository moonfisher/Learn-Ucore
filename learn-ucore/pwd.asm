
disk0/pwd：     文件格式 elf32-i386


Disassembly of section .text:

00800020 <__clone>:
#include "unistd.h"

.text
.globl __clone
__clone:                        # __clone(clone_flags, stack, fn, arg)
    pushl %ebp                  # maintain ebp chain
  800020:	55                   	push   %ebp
    movl %esp, %ebp
  800021:	89 e5                	mov    %esp,%ebp

    pushl %edx                  # save old registers
  800023:	52                   	push   %edx
    pushl %ecx
  800024:	51                   	push   %ecx
    pushl %ebx
  800025:	53                   	push   %ebx
    pushl %edi
  800026:	57                   	push   %edi

    movl 0x8(%ebp), %edx        # load clone_flags
  800027:	8b 55 08             	mov    0x8(%ebp),%edx
    movl 0xc(%ebp), %ecx        # load stack
  80002a:	8b 4d 0c             	mov    0xc(%ebp),%ecx
    movl 0x10(%ebp), %ebx       # load fn
  80002d:	8b 5d 10             	mov    0x10(%ebp),%ebx
    movl 0x14(%ebp), %edi       # load arg
  800030:	8b 7d 14             	mov    0x14(%ebp),%edi

    # sys_clone 函数里，内核只用到了 clone_flags 和 stack 两个参数
    # fn 和 arg 属于用户层参数，内核无需关心
    movl $SYS_clone, %eax       # load SYS_clone
  800033:	b8 05 00 00 00       	mov    $0x5,%eax
    int $T_SYSCALL              # syscall
  800038:	cd 80                	int    $0x80

    # clone 完成之后，父进程和子进程都会从这里返回
    cmpl $0x0, %eax             # pid ? child or parent ?
  80003a:	83 f8 00             	cmp    $0x0,%eax
    je 1f                       # eax == 0, goto 1;
  80003d:	74 06                	je     800045 <__clone+0x25>

    # parent
    popl %edi                   # restore registers
  80003f:	5f                   	pop    %edi
    popl %ebx
  800040:	5b                   	pop    %ebx
    popl %ecx
  800041:	59                   	pop    %ecx
    popl %edx
  800042:	5a                   	pop    %edx

    leave                       # restore ebp
  800043:	c9                   	leave  
    ret
  800044:	c3                   	ret    

    # child
1:
    # 从这里可以看到，子进程入口函数地址，以及父进程给子进程传递的参数
    # 都是在用户层控制的，内核并不关心入口函数地址和参数，内核只是创建了一个新的 task
    pushl %edi
  800045:	57                   	push   %edi
    call *%ebx                  # call fn(arg)
  800046:	ff d3                	call   *%ebx
    # 子进程退出之后的返回值也是通过 eax 返回
    movl %eax, %edx             # save exit_code
  800048:	89 c2                	mov    %eax,%edx
    movl $SYS_exit_thread, %eax # load SYS_exit_thread
  80004a:	b8 09 00 00 00       	mov    $0x9,%eax
    int $T_SYSCALL              # int SYS_exit_thread
  80004f:	cd 80                	int    $0x80

00800051 <spin>:

spin:                           # error ?
    jmp spin
  800051:	eb fe                	jmp    800051 <spin>

00800053 <opendir>:
#include "unistd.h"

DIR dir, *dirp = &dir;

DIR *opendir(const char *path)
{
  800053:	55                   	push   %ebp
  800054:	89 e5                	mov    %esp,%ebp
  800056:	53                   	push   %ebx
  800057:	83 ec 24             	sub    $0x24,%esp
    if ((dirp->fd = open(path, O_RDONLY)) < 0)
  80005a:	8b 1d 00 30 80 00    	mov    0x803000,%ebx
  800060:	83 ec 08             	sub    $0x8,%esp
  800063:	6a 00                	push   $0x0
  800065:	ff 75 08             	pushl  0x8(%ebp)
  800068:	e8 4b 01 00 00       	call   8001b8 <open>
  80006d:	83 c4 10             	add    $0x10,%esp
  800070:	89 03                	mov    %eax,(%ebx)
  800072:	8b 03                	mov    (%ebx),%eax
  800074:	85 c0                	test   %eax,%eax
  800076:	78 44                	js     8000bc <opendir+0x69>
    {
        goto failed;
    }
    struct stat __stat, *stat = &__stat;
  800078:	8d 45 e4             	lea    -0x1c(%ebp),%eax
  80007b:	89 45 f4             	mov    %eax,-0xc(%ebp)
    if (fstat(dirp->fd, stat) != 0 || !S_ISDIR(stat->st_mode))
  80007e:	a1 00 30 80 00       	mov    0x803000,%eax
  800083:	8b 00                	mov    (%eax),%eax
  800085:	83 ec 08             	sub    $0x8,%esp
  800088:	ff 75 f4             	pushl  -0xc(%ebp)
  80008b:	50                   	push   %eax
  80008c:	e8 ac 01 00 00       	call   80023d <fstat>
  800091:	83 c4 10             	add    $0x10,%esp
  800094:	85 c0                	test   %eax,%eax
  800096:	75 27                	jne    8000bf <opendir+0x6c>
  800098:	8b 45 f4             	mov    -0xc(%ebp),%eax
  80009b:	8b 00                	mov    (%eax),%eax
  80009d:	25 00 70 00 00       	and    $0x7000,%eax
  8000a2:	3d 00 20 00 00       	cmp    $0x2000,%eax
  8000a7:	75 16                	jne    8000bf <opendir+0x6c>
    {
        goto failed;
    }
    dirp->dirent.offset = 0;
  8000a9:	a1 00 30 80 00       	mov    0x803000,%eax
  8000ae:	c7 40 04 00 00 00 00 	movl   $0x0,0x4(%eax)
    return dirp;
  8000b5:	a1 00 30 80 00       	mov    0x803000,%eax
  8000ba:	eb 09                	jmp    8000c5 <opendir+0x72>
        goto failed;
  8000bc:	90                   	nop
  8000bd:	eb 01                	jmp    8000c0 <opendir+0x6d>
        goto failed;
  8000bf:	90                   	nop

failed:
    return NULL;
  8000c0:	b8 00 00 00 00       	mov    $0x0,%eax
}
  8000c5:	8b 5d fc             	mov    -0x4(%ebp),%ebx
  8000c8:	c9                   	leave  
  8000c9:	c3                   	ret    

008000ca <readdir>:

struct dirent *readdir(DIR *dirp)
{
  8000ca:	55                   	push   %ebp
  8000cb:	89 e5                	mov    %esp,%ebp
  8000cd:	83 ec 08             	sub    $0x8,%esp
    if (sys_getdirentry(dirp->fd, &(dirp->dirent)) == 0)
  8000d0:	8b 45 08             	mov    0x8(%ebp),%eax
  8000d3:	8d 50 04             	lea    0x4(%eax),%edx
  8000d6:	8b 45 08             	mov    0x8(%ebp),%eax
  8000d9:	8b 00                	mov    (%eax),%eax
  8000db:	83 ec 08             	sub    $0x8,%esp
  8000de:	52                   	push   %edx
  8000df:	50                   	push   %eax
  8000e0:	e8 ee 0c 00 00       	call   800dd3 <sys_getdirentry>
  8000e5:	83 c4 10             	add    $0x10,%esp
  8000e8:	85 c0                	test   %eax,%eax
  8000ea:	75 08                	jne    8000f4 <readdir+0x2a>
    {
        return &(dirp->dirent);
  8000ec:	8b 45 08             	mov    0x8(%ebp),%eax
  8000ef:	83 c0 04             	add    $0x4,%eax
  8000f2:	eb 05                	jmp    8000f9 <readdir+0x2f>
    }
    return NULL;
  8000f4:	b8 00 00 00 00       	mov    $0x0,%eax
}
  8000f9:	c9                   	leave  
  8000fa:	c3                   	ret    

008000fb <closedir>:

void closedir(DIR *dirp)
{
  8000fb:	55                   	push   %ebp
  8000fc:	89 e5                	mov    %esp,%ebp
  8000fe:	83 ec 08             	sub    $0x8,%esp
    close(dirp->fd);
  800101:	8b 45 08             	mov    0x8(%ebp),%eax
  800104:	8b 00                	mov    (%eax),%eax
  800106:	83 ec 0c             	sub    $0xc,%esp
  800109:	50                   	push   %eax
  80010a:	e8 c4 00 00 00       	call   8001d3 <close>
  80010f:	83 c4 10             	add    $0x10,%esp
}
  800112:	90                   	nop
  800113:	c9                   	leave  
  800114:	c3                   	ret    

00800115 <chdir>:

int chdir(const char *path)
{
  800115:	55                   	push   %ebp
  800116:	89 e5                	mov    %esp,%ebp
  800118:	83 ec 08             	sub    $0x8,%esp
    return sys_chdir(path);
  80011b:	83 ec 0c             	sub    $0xc,%esp
  80011e:	ff 75 08             	pushl  0x8(%ebp)
  800121:	e8 01 0d 00 00       	call   800e27 <sys_chdir>
  800126:	83 c4 10             	add    $0x10,%esp
}
  800129:	c9                   	leave  
  80012a:	c3                   	ret    

0080012b <getcwd>:

int getcwd(char *buffer, size_t len)
{
  80012b:	55                   	push   %ebp
  80012c:	89 e5                	mov    %esp,%ebp
  80012e:	83 ec 08             	sub    $0x8,%esp
    return sys_getcwd(buffer, len);
  800131:	83 ec 08             	sub    $0x8,%esp
  800134:	ff 75 0c             	pushl  0xc(%ebp)
  800137:	ff 75 08             	pushl  0x8(%ebp)
  80013a:	e8 7f 0c 00 00       	call   800dbe <sys_getcwd>
  80013f:	83 c4 10             	add    $0x10,%esp
}
  800142:	c9                   	leave  
  800143:	c3                   	ret    

00800144 <mkdir>:

int mkdir(const char *path)
{
  800144:	55                   	push   %ebp
  800145:	89 e5                	mov    %esp,%ebp
  800147:	83 ec 08             	sub    $0x8,%esp
    return sys_mkdir(path);
  80014a:	83 ec 0c             	sub    $0xc,%esp
  80014d:	ff 75 08             	pushl  0x8(%ebp)
  800150:	e8 ae 0c 00 00       	call   800e03 <sys_mkdir>
  800155:	83 c4 10             	add    $0x10,%esp
}
  800158:	c9                   	leave  
  800159:	c3                   	ret    

0080015a <rm>:

int rm(const char *path)
{
  80015a:	55                   	push   %ebp
  80015b:	89 e5                	mov    %esp,%ebp
  80015d:	83 ec 08             	sub    $0x8,%esp
    return sys_rm(path);
  800160:	83 ec 0c             	sub    $0xc,%esp
  800163:	ff 75 08             	pushl  0x8(%ebp)
  800166:	e8 aa 0c 00 00       	call   800e15 <sys_rm>
  80016b:	83 c4 10             	add    $0x10,%esp
}
  80016e:	c9                   	leave  
  80016f:	c3                   	ret    

00800170 <rename>:

int rename(const char *old_path, const char *new_path)
{
  800170:	55                   	push   %ebp
  800171:	89 e5                	mov    %esp,%ebp
  800173:	83 ec 08             	sub    $0x8,%esp
    return sys_rename(old_path, new_path);
  800176:	83 ec 08             	sub    $0x8,%esp
  800179:	ff 75 0c             	pushl  0xc(%ebp)
  80017c:	ff 75 08             	pushl  0x8(%ebp)
  80017f:	e8 b5 0c 00 00       	call   800e39 <sys_rename>
  800184:	83 c4 10             	add    $0x10,%esp
}
  800187:	c9                   	leave  
  800188:	c3                   	ret    

00800189 <link>:

int link(const char *old_path, const char *new_path)
{
  800189:	55                   	push   %ebp
  80018a:	89 e5                	mov    %esp,%ebp
  80018c:	83 ec 08             	sub    $0x8,%esp
    return sys_link(old_path, new_path);
  80018f:	83 ec 08             	sub    $0x8,%esp
  800192:	ff 75 0c             	pushl  0xc(%ebp)
  800195:	ff 75 08             	pushl  0x8(%ebp)
  800198:	e8 de 0c 00 00       	call   800e7b <sys_link>
  80019d:	83 c4 10             	add    $0x10,%esp
}
  8001a0:	c9                   	leave  
  8001a1:	c3                   	ret    

008001a2 <unlink>:

int unlink(const char *path)
{
  8001a2:	55                   	push   %ebp
  8001a3:	89 e5                	mov    %esp,%ebp
  8001a5:	83 ec 08             	sub    $0x8,%esp
    return sys_unlink(path);
  8001a8:	83 ec 0c             	sub    $0xc,%esp
  8001ab:	ff 75 08             	pushl  0x8(%ebp)
  8001ae:	e8 dd 0c 00 00       	call   800e90 <sys_unlink>
  8001b3:	83 c4 10             	add    $0x10,%esp
}
  8001b6:	c9                   	leave  
  8001b7:	c3                   	ret    

008001b8 <open>:
#include "stat.h"
#include "error.h"
#include "unistd.h"

int open(const char *path, uint32_t open_flags)
{
  8001b8:	55                   	push   %ebp
  8001b9:	89 e5                	mov    %esp,%ebp
  8001bb:	83 ec 08             	sub    $0x8,%esp
    return sys_open(path, open_flags, 0);
  8001be:	83 ec 04             	sub    $0x4,%esp
  8001c1:	6a 00                	push   $0x0
  8001c3:	ff 75 0c             	pushl  0xc(%ebp)
  8001c6:	ff 75 08             	pushl  0x8(%ebp)
  8001c9:	e8 57 0b 00 00       	call   800d25 <sys_open>
  8001ce:	83 c4 10             	add    $0x10,%esp
}
  8001d1:	c9                   	leave  
  8001d2:	c3                   	ret    

008001d3 <close>:

int close(int fd)
{
  8001d3:	55                   	push   %ebp
  8001d4:	89 e5                	mov    %esp,%ebp
  8001d6:	83 ec 08             	sub    $0x8,%esp
    return sys_close(fd);
  8001d9:	83 ec 0c             	sub    $0xc,%esp
  8001dc:	ff 75 08             	pushl  0x8(%ebp)
  8001df:	e8 59 0b 00 00       	call   800d3d <sys_close>
  8001e4:	83 c4 10             	add    $0x10,%esp
}
  8001e7:	c9                   	leave  
  8001e8:	c3                   	ret    

008001e9 <read>:

int read(int fd, void *base, size_t len)
{
  8001e9:	55                   	push   %ebp
  8001ea:	89 e5                	mov    %esp,%ebp
  8001ec:	83 ec 08             	sub    $0x8,%esp
    return sys_read(fd, base, len);
  8001ef:	83 ec 04             	sub    $0x4,%esp
  8001f2:	ff 75 10             	pushl  0x10(%ebp)
  8001f5:	ff 75 0c             	pushl  0xc(%ebp)
  8001f8:	ff 75 08             	pushl  0x8(%ebp)
  8001fb:	e8 4f 0b 00 00       	call   800d4f <sys_read>
  800200:	83 c4 10             	add    $0x10,%esp
}
  800203:	c9                   	leave  
  800204:	c3                   	ret    

00800205 <write>:

int write(int fd, void *base, size_t len)
{
  800205:	55                   	push   %ebp
  800206:	89 e5                	mov    %esp,%ebp
  800208:	83 ec 08             	sub    $0x8,%esp
    return sys_write(fd, base, len);
  80020b:	83 ec 04             	sub    $0x4,%esp
  80020e:	ff 75 10             	pushl  0x10(%ebp)
  800211:	ff 75 0c             	pushl  0xc(%ebp)
  800214:	ff 75 08             	pushl  0x8(%ebp)
  800217:	e8 4b 0b 00 00       	call   800d67 <sys_write>
  80021c:	83 c4 10             	add    $0x10,%esp
}
  80021f:	c9                   	leave  
  800220:	c3                   	ret    

00800221 <seek>:

int seek(int fd, off_t pos, int whence)
{
  800221:	55                   	push   %ebp
  800222:	89 e5                	mov    %esp,%ebp
  800224:	83 ec 08             	sub    $0x8,%esp
    return sys_seek(fd, pos, whence);
  800227:	83 ec 04             	sub    $0x4,%esp
  80022a:	ff 75 10             	pushl  0x10(%ebp)
  80022d:	ff 75 0c             	pushl  0xc(%ebp)
  800230:	ff 75 08             	pushl  0x8(%ebp)
  800233:	e8 47 0b 00 00       	call   800d7f <sys_seek>
  800238:	83 c4 10             	add    $0x10,%esp
}
  80023b:	c9                   	leave  
  80023c:	c3                   	ret    

0080023d <fstat>:

int fstat(int fd, struct stat *stat)
{
  80023d:	55                   	push   %ebp
  80023e:	89 e5                	mov    %esp,%ebp
  800240:	83 ec 08             	sub    $0x8,%esp
    return sys_fstat(fd, stat);
  800243:	83 ec 08             	sub    $0x8,%esp
  800246:	ff 75 0c             	pushl  0xc(%ebp)
  800249:	ff 75 08             	pushl  0x8(%ebp)
  80024c:	e8 46 0b 00 00       	call   800d97 <sys_fstat>
  800251:	83 c4 10             	add    $0x10,%esp
}
  800254:	c9                   	leave  
  800255:	c3                   	ret    

00800256 <fsync>:

int fsync(int fd)
{
  800256:	55                   	push   %ebp
  800257:	89 e5                	mov    %esp,%ebp
  800259:	83 ec 08             	sub    $0x8,%esp
    return sys_fsync(fd);
  80025c:	83 ec 0c             	sub    $0xc,%esp
  80025f:	ff 75 08             	pushl  0x8(%ebp)
  800262:	e8 45 0b 00 00       	call   800dac <sys_fsync>
  800267:	83 c4 10             	add    $0x10,%esp
}
  80026a:	c9                   	leave  
  80026b:	c3                   	ret    

0080026c <dup2>:

int dup2(int fd1, int fd2)
{
  80026c:	55                   	push   %ebp
  80026d:	89 e5                	mov    %esp,%ebp
  80026f:	83 ec 08             	sub    $0x8,%esp
    return sys_dup(fd1, fd2);
  800272:	83 ec 08             	sub    $0x8,%esp
  800275:	ff 75 0c             	pushl  0xc(%ebp)
  800278:	ff 75 08             	pushl  0x8(%ebp)
  80027b:	e8 6b 0b 00 00       	call   800deb <sys_dup>
  800280:	83 c4 10             	add    $0x10,%esp
}
  800283:	c9                   	leave  
  800284:	c3                   	ret    

00800285 <pipe>:

int pipe(int *fd_store)
{
  800285:	55                   	push   %ebp
  800286:	89 e5                	mov    %esp,%ebp
  800288:	83 ec 08             	sub    $0x8,%esp
    return sys_pipe(fd_store);
  80028b:	83 ec 0c             	sub    $0xc,%esp
  80028e:	ff 75 08             	pushl  0x8(%ebp)
  800291:	e8 b8 0b 00 00       	call   800e4e <sys_pipe>
  800296:	83 c4 10             	add    $0x10,%esp
}
  800299:	c9                   	leave  
  80029a:	c3                   	ret    

0080029b <mkfifo>:

int mkfifo(const char *name, uint32_t open_flags)
{
  80029b:	55                   	push   %ebp
  80029c:	89 e5                	mov    %esp,%ebp
  80029e:	83 ec 08             	sub    $0x8,%esp
    return sys_mkfifo(name, open_flags);
  8002a1:	83 ec 08             	sub    $0x8,%esp
  8002a4:	ff 75 0c             	pushl  0xc(%ebp)
  8002a7:	ff 75 08             	pushl  0x8(%ebp)
  8002aa:	e8 b4 0b 00 00       	call   800e63 <sys_mkfifo>
  8002af:	83 c4 10             	add    $0x10,%esp
}
  8002b2:	c9                   	leave  
  8002b3:	c3                   	ret    

008002b4 <transmode>:

static char transmode(struct stat *stat)
{
  8002b4:	55                   	push   %ebp
  8002b5:	89 e5                	mov    %esp,%ebp
  8002b7:	83 ec 10             	sub    $0x10,%esp
    uint32_t mode = stat->st_mode;
  8002ba:	8b 45 08             	mov    0x8(%ebp),%eax
  8002bd:	8b 00                	mov    (%eax),%eax
  8002bf:	89 45 fc             	mov    %eax,-0x4(%ebp)
    if (S_ISREG(mode)) return 'r';
  8002c2:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8002c5:	25 00 70 00 00       	and    $0x7000,%eax
  8002ca:	3d 00 10 00 00       	cmp    $0x1000,%eax
  8002cf:	75 04                	jne    8002d5 <transmode+0x21>
  8002d1:	b0 72                	mov    $0x72,%al
  8002d3:	eb 4e                	jmp    800323 <transmode+0x6f>
    if (S_ISDIR(mode)) return 'd';
  8002d5:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8002d8:	25 00 70 00 00       	and    $0x7000,%eax
  8002dd:	3d 00 20 00 00       	cmp    $0x2000,%eax
  8002e2:	75 04                	jne    8002e8 <transmode+0x34>
  8002e4:	b0 64                	mov    $0x64,%al
  8002e6:	eb 3b                	jmp    800323 <transmode+0x6f>
    if (S_ISLNK(mode)) return 'l';
  8002e8:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8002eb:	25 00 70 00 00       	and    $0x7000,%eax
  8002f0:	3d 00 30 00 00       	cmp    $0x3000,%eax
  8002f5:	75 04                	jne    8002fb <transmode+0x47>
  8002f7:	b0 6c                	mov    $0x6c,%al
  8002f9:	eb 28                	jmp    800323 <transmode+0x6f>
    if (S_ISCHR(mode)) return 'c';
  8002fb:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8002fe:	25 00 70 00 00       	and    $0x7000,%eax
  800303:	3d 00 40 00 00       	cmp    $0x4000,%eax
  800308:	75 04                	jne    80030e <transmode+0x5a>
  80030a:	b0 63                	mov    $0x63,%al
  80030c:	eb 15                	jmp    800323 <transmode+0x6f>
    if (S_ISBLK(mode)) return 'b';
  80030e:	8b 45 fc             	mov    -0x4(%ebp),%eax
  800311:	25 00 70 00 00       	and    $0x7000,%eax
  800316:	3d 00 50 00 00       	cmp    $0x5000,%eax
  80031b:	75 04                	jne    800321 <transmode+0x6d>
  80031d:	b0 62                	mov    $0x62,%al
  80031f:	eb 02                	jmp    800323 <transmode+0x6f>
    return '-';
  800321:	b0 2d                	mov    $0x2d,%al
}
  800323:	c9                   	leave  
  800324:	c3                   	ret    

00800325 <print_stat>:

void print_stat(const char *name, int fd, struct stat *stat)
{
  800325:	55                   	push   %ebp
  800326:	89 e5                	mov    %esp,%ebp
  800328:	83 ec 08             	sub    $0x8,%esp
    cprintf("[%03d] %s\n", fd, name);
  80032b:	83 ec 04             	sub    $0x4,%esp
  80032e:	ff 75 08             	pushl  0x8(%ebp)
  800331:	ff 75 0c             	pushl  0xc(%ebp)
  800334:	68 60 28 80 00       	push   $0x802860
  800339:	e8 44 07 00 00       	call   800a82 <cprintf>
  80033e:	83 c4 10             	add    $0x10,%esp
    cprintf("    mode    : %c\n", transmode(stat));
  800341:	83 ec 0c             	sub    $0xc,%esp
  800344:	ff 75 10             	pushl  0x10(%ebp)
  800347:	e8 68 ff ff ff       	call   8002b4 <transmode>
  80034c:	83 c4 10             	add    $0x10,%esp
  80034f:	0f be c0             	movsbl %al,%eax
  800352:	83 ec 08             	sub    $0x8,%esp
  800355:	50                   	push   %eax
  800356:	68 6b 28 80 00       	push   $0x80286b
  80035b:	e8 22 07 00 00       	call   800a82 <cprintf>
  800360:	83 c4 10             	add    $0x10,%esp
    cprintf("    links   : %lu\n", stat->st_nlinks);
  800363:	8b 45 10             	mov    0x10(%ebp),%eax
  800366:	8b 40 04             	mov    0x4(%eax),%eax
  800369:	83 ec 08             	sub    $0x8,%esp
  80036c:	50                   	push   %eax
  80036d:	68 7d 28 80 00       	push   $0x80287d
  800372:	e8 0b 07 00 00       	call   800a82 <cprintf>
  800377:	83 c4 10             	add    $0x10,%esp
    cprintf("    blocks  : %lu\n", stat->st_blocks);
  80037a:	8b 45 10             	mov    0x10(%ebp),%eax
  80037d:	8b 40 08             	mov    0x8(%eax),%eax
  800380:	83 ec 08             	sub    $0x8,%esp
  800383:	50                   	push   %eax
  800384:	68 90 28 80 00       	push   $0x802890
  800389:	e8 f4 06 00 00       	call   800a82 <cprintf>
  80038e:	83 c4 10             	add    $0x10,%esp
    cprintf("    size    : %lu\n", stat->st_size);
  800391:	8b 45 10             	mov    0x10(%ebp),%eax
  800394:	8b 40 0c             	mov    0xc(%eax),%eax
  800397:	83 ec 08             	sub    $0x8,%esp
  80039a:	50                   	push   %eax
  80039b:	68 a3 28 80 00       	push   $0x8028a3
  8003a0:	e8 dd 06 00 00       	call   800a82 <cprintf>
  8003a5:	83 c4 10             	add    $0x10,%esp
}
  8003a8:	90                   	nop
  8003a9:	c9                   	leave  
  8003aa:	c3                   	ret    

008003ab <_start>:
# 常见的 main 函数并不是真正的入口
.text
.globl _start
_start:
    # set ebp for backtrace
    movl $0x0, %ebp
  8003ab:	bd 00 00 00 00       	mov    $0x0,%ebp

    # load argc and argv
    # 从堆栈里先取出 argc 和 argv，这个之前在 load_icode 里设置好了
    movl (%esp), %ebx
  8003b0:	8b 1c 24             	mov    (%esp),%ebx
    lea 0x4(%esp), %ecx
  8003b3:	8d 4c 24 04          	lea    0x4(%esp),%ecx

    # move down the esp register
    # since it may cause page fault in backtrace
    subl $0x20, %esp
  8003b7:	83 ec 20             	sub    $0x20,%esp

    # save argc and argv on stack
    # ecx 存放的 argc，ebx 地址指向 argv
    pushl %ecx
  8003ba:	51                   	push   %ecx
    pushl %ebx
  8003bb:	53                   	push   %ebx

    # call user-program function
    call umain
  8003bc:	e8 cb 14 00 00       	call   80188c <umain>
1:  jmp 1b
  8003c1:	eb fe                	jmp    8003c1 <_start+0x16>

008003c3 <try_lock>:
{
    *l = 0;
}

static inline bool try_lock(lock_t *l)
{
  8003c3:	55                   	push   %ebp
  8003c4:	89 e5                	mov    %esp,%ebp
  8003c6:	83 ec 10             	sub    $0x10,%esp
  8003c9:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%ebp)
  8003d0:	8b 45 08             	mov    0x8(%ebp),%eax
  8003d3:	89 45 f8             	mov    %eax,-0x8(%ebp)
 * @addr:   the address to count from
 * */
static inline bool test_and_set_bit(int nr, volatile void *addr)
{
    int oldbit;
    asm volatile ("btsl %2, %1; sbbl %0, %0" : "=r" (oldbit), "=m" (*(volatile long *)addr) : "Ir" (nr) : "memory");
  8003d6:	8b 55 f8             	mov    -0x8(%ebp),%edx
  8003d9:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8003dc:	0f ab 02             	bts    %eax,(%edx)
  8003df:	19 c0                	sbb    %eax,%eax
  8003e1:	89 45 f4             	mov    %eax,-0xc(%ebp)
    return oldbit != 0;
  8003e4:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
  8003e8:	0f 95 c0             	setne  %al
  8003eb:	0f b6 c0             	movzbl %al,%eax
    return test_and_set_bit(0, l);
  8003ee:	90                   	nop
}
  8003ef:	c9                   	leave  
  8003f0:	c3                   	ret    

008003f1 <lock>:

static inline void lock(lock_t *l)
{
  8003f1:	55                   	push   %ebp
  8003f2:	89 e5                	mov    %esp,%ebp
  8003f4:	83 ec 18             	sub    $0x18,%esp
    if (try_lock(l))
  8003f7:	ff 75 08             	pushl  0x8(%ebp)
  8003fa:	e8 c4 ff ff ff       	call   8003c3 <try_lock>
  8003ff:	83 c4 04             	add    $0x4,%esp
  800402:	85 c0                	test   %eax,%eax
  800404:	74 3b                	je     800441 <lock+0x50>
    {
        int step = 0;
  800406:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
        do {
            yield();
  80040d:	e8 6f 10 00 00       	call   801481 <yield>
            if (++ step == 100)
  800412:	ff 45 f4             	incl   -0xc(%ebp)
  800415:	83 7d f4 64          	cmpl   $0x64,-0xc(%ebp)
  800419:	75 14                	jne    80042f <lock+0x3e>
            {
                step = 0;
  80041b:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
                sleep(10);
  800422:	83 ec 0c             	sub    $0xc,%esp
  800425:	6a 0a                	push   $0xa
  800427:	e8 fc 10 00 00       	call   801528 <sleep>
  80042c:	83 c4 10             	add    $0x10,%esp
            }
        } while (try_lock(l));
  80042f:	83 ec 0c             	sub    $0xc,%esp
  800432:	ff 75 08             	pushl  0x8(%ebp)
  800435:	e8 89 ff ff ff       	call   8003c3 <try_lock>
  80043a:	83 c4 10             	add    $0x10,%esp
  80043d:	85 c0                	test   %eax,%eax
  80043f:	75 cc                	jne    80040d <lock+0x1c>
    }
}
  800441:	90                   	nop
  800442:	c9                   	leave  
  800443:	c3                   	ret    

00800444 <unlock>:

static inline void unlock(lock_t *l)
{
  800444:	55                   	push   %ebp
  800445:	89 e5                	mov    %esp,%ebp
  800447:	83 ec 10             	sub    $0x10,%esp
  80044a:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%ebp)
  800451:	8b 45 08             	mov    0x8(%ebp),%eax
  800454:	89 45 f8             	mov    %eax,-0x8(%ebp)
 * @addr:   the address to count from
 * */
static inline bool test_and_clear_bit(int nr, volatile void *addr)
{
    int oldbit;
    asm volatile ("btrl %2, %1; sbbl %0, %0" : "=r" (oldbit), "=m" (*(volatile long *)addr) : "Ir" (nr) : "memory");
  800457:	8b 55 f8             	mov    -0x8(%ebp),%edx
  80045a:	8b 45 fc             	mov    -0x4(%ebp),%eax
  80045d:	0f b3 02             	btr    %eax,(%edx)
  800460:	19 c0                	sbb    %eax,%eax
  800462:	89 45 f4             	mov    %eax,-0xc(%ebp)
    return oldbit != 0;
  800465:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
    test_and_clear_bit(0, l);
}
  800469:	90                   	nop
  80046a:	c9                   	leave  
  80046b:	c3                   	ret    

0080046c <lock_malloc>:
static lock_t mem_lock = INIT_LOCK;

static void free_locked(void *ap);

static inline void lock_malloc(void)
{
  80046c:	55                   	push   %ebp
  80046d:	89 e5                	mov    %esp,%ebp
  80046f:	83 ec 08             	sub    $0x8,%esp
    lock_fork();
  800472:	e8 46 0f 00 00       	call   8013bd <lock_fork>
    lock(&mem_lock);
  800477:	83 ec 0c             	sub    $0xc,%esp
  80047a:	68 04 31 80 00       	push   $0x803104
  80047f:	e8 6d ff ff ff       	call   8003f1 <lock>
  800484:	83 c4 10             	add    $0x10,%esp
}
  800487:	90                   	nop
  800488:	c9                   	leave  
  800489:	c3                   	ret    

0080048a <unlock_malloc>:

static inline void unlock_malloc(void)
{
  80048a:	55                   	push   %ebp
  80048b:	89 e5                	mov    %esp,%ebp
  80048d:	83 ec 08             	sub    $0x8,%esp
    unlock(&mem_lock);
  800490:	68 04 31 80 00       	push   $0x803104
  800495:	e8 aa ff ff ff       	call   800444 <unlock>
  80049a:	83 c4 04             	add    $0x4,%esp
    unlock_fork();
  80049d:	e8 34 0f 00 00       	call   8013d6 <unlock_fork>
}
  8004a2:	90                   	nop
  8004a3:	c9                   	leave  
  8004a4:	c3                   	ret    

008004a5 <morecore_brk_locked>:

static bool morecore_brk_locked(size_t nu)
{
  8004a5:	55                   	push   %ebp
  8004a6:	89 e5                	mov    %esp,%ebp
  8004a8:	83 ec 18             	sub    $0x18,%esp
    static uintptr_t brk = 0;
    if (brk == 0)
  8004ab:	a1 08 31 80 00       	mov    0x803108,%eax
  8004b0:	85 c0                	test   %eax,%eax
  8004b2:	75 24                	jne    8004d8 <morecore_brk_locked+0x33>
    {
        if (sys_brk(&brk) != 0 || brk == 0)
  8004b4:	83 ec 0c             	sub    $0xc,%esp
  8004b7:	68 08 31 80 00       	push   $0x803108
  8004bc:	e8 0d 08 00 00       	call   800cce <sys_brk>
  8004c1:	83 c4 10             	add    $0x10,%esp
  8004c4:	85 c0                	test   %eax,%eax
  8004c6:	75 09                	jne    8004d1 <morecore_brk_locked+0x2c>
  8004c8:	a1 08 31 80 00       	mov    0x803108,%eax
  8004cd:	85 c0                	test   %eax,%eax
  8004cf:	75 07                	jne    8004d8 <morecore_brk_locked+0x33>
        {
            return 0;
  8004d1:	b8 00 00 00 00       	mov    $0x0,%eax
  8004d6:	eb 7f                	jmp    800557 <morecore_brk_locked+0xb2>
        }
    }
    uintptr_t newbrk = brk + nu + sizeof(header_t);
  8004d8:	8b 15 08 31 80 00    	mov    0x803108,%edx
  8004de:	8b 45 08             	mov    0x8(%ebp),%eax
  8004e1:	01 d0                	add    %edx,%eax
  8004e3:	83 c0 40             	add    $0x40,%eax
  8004e6:	89 45 f0             	mov    %eax,-0x10(%ebp)
    if (sys_brk(&newbrk) != 0 || newbrk <= brk)
  8004e9:	83 ec 0c             	sub    $0xc,%esp
  8004ec:	8d 45 f0             	lea    -0x10(%ebp),%eax
  8004ef:	50                   	push   %eax
  8004f0:	e8 d9 07 00 00       	call   800cce <sys_brk>
  8004f5:	83 c4 10             	add    $0x10,%esp
  8004f8:	85 c0                	test   %eax,%eax
  8004fa:	75 0c                	jne    800508 <morecore_brk_locked+0x63>
  8004fc:	8b 55 f0             	mov    -0x10(%ebp),%edx
  8004ff:	a1 08 31 80 00       	mov    0x803108,%eax
  800504:	39 c2                	cmp    %eax,%edx
  800506:	77 07                	ja     80050f <morecore_brk_locked+0x6a>
    {
        return 0;
  800508:	b8 00 00 00 00       	mov    $0x0,%eax
  80050d:	eb 48                	jmp    800557 <morecore_brk_locked+0xb2>
    }
    
    header_t *p = (header_t *)brk;
  80050f:	a1 08 31 80 00       	mov    0x803108,%eax
  800514:	89 45 f4             	mov    %eax,-0xc(%ebp)
    p->s.size = (newbrk - brk) / sizeof(header_t);
  800517:	8b 55 f0             	mov    -0x10(%ebp),%edx
  80051a:	a1 08 31 80 00       	mov    0x803108,%eax
  80051f:	29 c2                	sub    %eax,%edx
  800521:	89 d0                	mov    %edx,%eax
  800523:	c1 e8 06             	shr    $0x6,%eax
  800526:	89 c2                	mov    %eax,%edx
  800528:	8b 45 f4             	mov    -0xc(%ebp),%eax
  80052b:	89 50 04             	mov    %edx,0x4(%eax)
    p->s.type = 0;
  80052e:	8b 45 f4             	mov    -0xc(%ebp),%eax
  800531:	c7 40 08 00 00 00 00 	movl   $0x0,0x8(%eax)
    free_locked((void *)(p + 1));
  800538:	8b 45 f4             	mov    -0xc(%ebp),%eax
  80053b:	83 c0 40             	add    $0x40,%eax
  80053e:	83 ec 0c             	sub    $0xc,%esp
  800541:	50                   	push   %eax
  800542:	e8 bc 01 00 00       	call   800703 <free_locked>
  800547:	83 c4 10             	add    $0x10,%esp
    brk = newbrk;
  80054a:	8b 45 f0             	mov    -0x10(%ebp),%eax
  80054d:	a3 08 31 80 00       	mov    %eax,0x803108
    return 1;
  800552:	b8 01 00 00 00       	mov    $0x1,%eax
}
  800557:	c9                   	leave  
  800558:	c3                   	ret    

00800559 <morecore_shmem_locked>:

static bool morecore_shmem_locked(size_t nu)
{
  800559:	55                   	push   %ebp
  80055a:	89 e5                	mov    %esp,%ebp
  80055c:	83 ec 18             	sub    $0x18,%esp
    size_t size = ((nu * sizeof(header_t) + 0xfff) & (~0xfff));
  80055f:	8b 45 08             	mov    0x8(%ebp),%eax
  800562:	c1 e0 06             	shl    $0x6,%eax
  800565:	05 ff 0f 00 00       	add    $0xfff,%eax
  80056a:	25 00 f0 ff ff       	and    $0xfffff000,%eax
  80056f:	89 45 f4             	mov    %eax,-0xc(%ebp)
    uintptr_t mem = 0;
  800572:	c7 45 ec 00 00 00 00 	movl   $0x0,-0x14(%ebp)
    if (sys_shmem(&mem, size, MMAP_WRITE) != 0 || mem == 0)
  800579:	83 ec 04             	sub    $0x4,%esp
  80057c:	68 00 01 00 00       	push   $0x100
  800581:	ff 75 f4             	pushl  -0xc(%ebp)
  800584:	8d 45 ec             	lea    -0x14(%ebp),%eax
  800587:	50                   	push   %eax
  800588:	e8 c0 06 00 00       	call   800c4d <sys_shmem>
  80058d:	83 c4 10             	add    $0x10,%esp
  800590:	85 c0                	test   %eax,%eax
  800592:	75 07                	jne    80059b <morecore_shmem_locked+0x42>
  800594:	8b 45 ec             	mov    -0x14(%ebp),%eax
  800597:	85 c0                	test   %eax,%eax
  800599:	75 07                	jne    8005a2 <morecore_shmem_locked+0x49>
    {
        return 0;
  80059b:	b8 00 00 00 00       	mov    $0x0,%eax
  8005a0:	eb 35                	jmp    8005d7 <morecore_shmem_locked+0x7e>
    }
    
    header_t *p = (header_t *)mem;
  8005a2:	8b 45 ec             	mov    -0x14(%ebp),%eax
  8005a5:	89 45 f0             	mov    %eax,-0x10(%ebp)
    p->s.size = size / sizeof(header_t);
  8005a8:	8b 45 f4             	mov    -0xc(%ebp),%eax
  8005ab:	c1 e8 06             	shr    $0x6,%eax
  8005ae:	89 c2                	mov    %eax,%edx
  8005b0:	8b 45 f0             	mov    -0x10(%ebp),%eax
  8005b3:	89 50 04             	mov    %edx,0x4(%eax)
    p->s.type = 1;
  8005b6:	8b 45 f0             	mov    -0x10(%ebp),%eax
  8005b9:	c7 40 08 01 00 00 00 	movl   $0x1,0x8(%eax)
    free_locked((void *)(p + 1));
  8005c0:	8b 45 f0             	mov    -0x10(%ebp),%eax
  8005c3:	83 c0 40             	add    $0x40,%eax
  8005c6:	83 ec 0c             	sub    $0xc,%esp
  8005c9:	50                   	push   %eax
  8005ca:	e8 34 01 00 00       	call   800703 <free_locked>
  8005cf:	83 c4 10             	add    $0x10,%esp
    return 1;
  8005d2:	b8 01 00 00 00       	mov    $0x1,%eax
}
  8005d7:	c9                   	leave  
  8005d8:	c3                   	ret    

008005d9 <malloc_locked>:

static void *malloc_locked(size_t size, bool type)
{
  8005d9:	55                   	push   %ebp
  8005da:	89 e5                	mov    %esp,%ebp
  8005dc:	83 ec 28             	sub    $0x28,%esp
    static_assert(sizeof(header_t) == 0x40);
    header_t *p, *prevp;
    size_t nunits;

    // make sure that type is 0 or 1
    if (type)
  8005df:	83 7d 0c 00          	cmpl   $0x0,0xc(%ebp)
  8005e3:	74 07                	je     8005ec <malloc_locked+0x13>
    {
        type = 1;
  8005e5:	c7 45 0c 01 00 00 00 	movl   $0x1,0xc(%ebp)
    }

    nunits = (size + sizeof(header_t) - 1) / sizeof(header_t) + 1;
  8005ec:	8b 45 08             	mov    0x8(%ebp),%eax
  8005ef:	83 c0 3f             	add    $0x3f,%eax
  8005f2:	c1 e8 06             	shr    $0x6,%eax
  8005f5:	40                   	inc    %eax
  8005f6:	89 45 ec             	mov    %eax,-0x14(%ebp)
    if ((prevp = freep) == NULL)
  8005f9:	a1 00 31 80 00       	mov    0x803100,%eax
  8005fe:	89 45 f0             	mov    %eax,-0x10(%ebp)
  800601:	83 7d f0 00          	cmpl   $0x0,-0x10(%ebp)
  800605:	75 23                	jne    80062a <malloc_locked+0x51>
    {
        //初始化
        base.s.ptr = freep = prevp = &base;
  800607:	c7 45 f0 c0 30 80 00 	movl   $0x8030c0,-0x10(%ebp)
  80060e:	8b 45 f0             	mov    -0x10(%ebp),%eax
  800611:	a3 00 31 80 00       	mov    %eax,0x803100
  800616:	a1 00 31 80 00       	mov    0x803100,%eax
  80061b:	a3 c0 30 80 00       	mov    %eax,0x8030c0
        base.s.size = 0;
  800620:	c7 05 c4 30 80 00 00 	movl   $0x0,0x8030c4
  800627:	00 00 00 
    }

    for (p = prevp->s.ptr; ; prevp = p, p = p->s.ptr)
  80062a:	8b 45 f0             	mov    -0x10(%ebp),%eax
  80062d:	8b 00                	mov    (%eax),%eax
  80062f:	89 45 f4             	mov    %eax,-0xc(%ebp)
    {
        // p表示下一个 header_t block
        // prevp表示
        if (p->s.type == type && p->s.size >= nunits)
  800632:	8b 45 f4             	mov    -0xc(%ebp),%eax
  800635:	8b 40 08             	mov    0x8(%eax),%eax
  800638:	39 45 0c             	cmp    %eax,0xc(%ebp)
  80063b:	75 79                	jne    8006b6 <malloc_locked+0xdd>
  80063d:	8b 45 f4             	mov    -0xc(%ebp),%eax
  800640:	8b 40 04             	mov    0x4(%eax),%eax
  800643:	39 45 ec             	cmp    %eax,-0x14(%ebp)
  800646:	77 6e                	ja     8006b6 <malloc_locked+0xdd>
        {
            //当前还有
            if (p->s.size == nunits)
  800648:	8b 45 f4             	mov    -0xc(%ebp),%eax
  80064b:	8b 40 04             	mov    0x4(%eax),%eax
  80064e:	39 45 ec             	cmp    %eax,-0x14(%ebp)
  800651:	75 0c                	jne    80065f <malloc_locked+0x86>
            {
                prevp->s.ptr = p->s.ptr;
  800653:	8b 45 f4             	mov    -0xc(%ebp),%eax
  800656:	8b 10                	mov    (%eax),%edx
  800658:	8b 45 f0             	mov    -0x10(%ebp),%eax
  80065b:	89 10                	mov    %edx,(%eax)
  80065d:	eb 47                	jmp    8006a6 <malloc_locked+0xcd>
            }
            else
            {
                header_t *np = prevp->s.ptr = (p + nunits);
  80065f:	8b 45 ec             	mov    -0x14(%ebp),%eax
  800662:	c1 e0 06             	shl    $0x6,%eax
  800665:	89 c2                	mov    %eax,%edx
  800667:	8b 45 f4             	mov    -0xc(%ebp),%eax
  80066a:	01 c2                	add    %eax,%edx
  80066c:	8b 45 f0             	mov    -0x10(%ebp),%eax
  80066f:	89 10                	mov    %edx,(%eax)
  800671:	8b 45 f0             	mov    -0x10(%ebp),%eax
  800674:	8b 00                	mov    (%eax),%eax
  800676:	89 45 e8             	mov    %eax,-0x18(%ebp)
                np->s.ptr = p->s.ptr;
  800679:	8b 45 f4             	mov    -0xc(%ebp),%eax
  80067c:	8b 10                	mov    (%eax),%edx
  80067e:	8b 45 e8             	mov    -0x18(%ebp),%eax
  800681:	89 10                	mov    %edx,(%eax)
                np->s.size = p->s.size - nunits;
  800683:	8b 45 f4             	mov    -0xc(%ebp),%eax
  800686:	8b 40 04             	mov    0x4(%eax),%eax
  800689:	2b 45 ec             	sub    -0x14(%ebp),%eax
  80068c:	89 c2                	mov    %eax,%edx
  80068e:	8b 45 e8             	mov    -0x18(%ebp),%eax
  800691:	89 50 04             	mov    %edx,0x4(%eax)
                np->s.type = type;
  800694:	8b 45 e8             	mov    -0x18(%ebp),%eax
  800697:	8b 55 0c             	mov    0xc(%ebp),%edx
  80069a:	89 50 08             	mov    %edx,0x8(%eax)
                p->s.size = nunits;
  80069d:	8b 45 f4             	mov    -0xc(%ebp),%eax
  8006a0:	8b 55 ec             	mov    -0x14(%ebp),%edx
  8006a3:	89 50 04             	mov    %edx,0x4(%eax)
            }
            freep = prevp;
  8006a6:	8b 45 f0             	mov    -0x10(%ebp),%eax
  8006a9:	a3 00 31 80 00       	mov    %eax,0x803100
            return (void *)(p + 1);
  8006ae:	8b 45 f4             	mov    -0xc(%ebp),%eax
  8006b1:	83 c0 40             	add    $0x40,%eax
  8006b4:	eb 4b                	jmp    800701 <malloc_locked+0x128>
        }
        if (p == freep)
  8006b6:	a1 00 31 80 00       	mov    0x803100,%eax
  8006bb:	39 45 f4             	cmp    %eax,-0xc(%ebp)
  8006be:	75 2e                	jne    8006ee <malloc_locked+0x115>
        {
            //需要重新分配
            bool (*morecore_locked)(size_t nu);
            morecore_locked = (!type) ? morecore_brk_locked : morecore_shmem_locked;
  8006c0:	83 7d 0c 00          	cmpl   $0x0,0xc(%ebp)
  8006c4:	75 07                	jne    8006cd <malloc_locked+0xf4>
  8006c6:	b8 a5 04 80 00       	mov    $0x8004a5,%eax
  8006cb:	eb 05                	jmp    8006d2 <malloc_locked+0xf9>
  8006cd:	b8 59 05 80 00       	mov    $0x800559,%eax
  8006d2:	89 45 e4             	mov    %eax,-0x1c(%ebp)
            if (!morecore_locked(nunits))
  8006d5:	83 ec 0c             	sub    $0xc,%esp
  8006d8:	ff 75 ec             	pushl  -0x14(%ebp)
  8006db:	8b 45 e4             	mov    -0x1c(%ebp),%eax
  8006de:	ff d0                	call   *%eax
  8006e0:	83 c4 10             	add    $0x10,%esp
  8006e3:	85 c0                	test   %eax,%eax
  8006e5:	75 07                	jne    8006ee <malloc_locked+0x115>
            {
                return NULL;
  8006e7:	b8 00 00 00 00       	mov    $0x0,%eax
  8006ec:	eb 13                	jmp    800701 <malloc_locked+0x128>
    for (p = prevp->s.ptr; ; prevp = p, p = p->s.ptr)
  8006ee:	8b 45 f4             	mov    -0xc(%ebp),%eax
  8006f1:	89 45 f0             	mov    %eax,-0x10(%ebp)
  8006f4:	8b 45 f4             	mov    -0xc(%ebp),%eax
  8006f7:	8b 00                	mov    (%eax),%eax
  8006f9:	89 45 f4             	mov    %eax,-0xc(%ebp)
        if (p->s.type == type && p->s.size >= nunits)
  8006fc:	e9 31 ff ff ff       	jmp    800632 <malloc_locked+0x59>
            }
        }
    }
}
  800701:	c9                   	leave  
  800702:	c3                   	ret    

00800703 <free_locked>:

static void free_locked(void *ap)
{
  800703:	55                   	push   %ebp
  800704:	89 e5                	mov    %esp,%ebp
  800706:	83 ec 10             	sub    $0x10,%esp
    header_t *bp = ((header_t *)ap) - 1, *p;
  800709:	8b 45 08             	mov    0x8(%ebp),%eax
  80070c:	83 e8 40             	sub    $0x40,%eax
  80070f:	89 45 f8             	mov    %eax,-0x8(%ebp)

    for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
  800712:	a1 00 31 80 00       	mov    0x803100,%eax
  800717:	89 45 fc             	mov    %eax,-0x4(%ebp)
  80071a:	eb 24                	jmp    800740 <free_locked+0x3d>
    {
        if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
  80071c:	8b 45 fc             	mov    -0x4(%ebp),%eax
  80071f:	8b 00                	mov    (%eax),%eax
  800721:	39 45 fc             	cmp    %eax,-0x4(%ebp)
  800724:	72 12                	jb     800738 <free_locked+0x35>
  800726:	8b 45 f8             	mov    -0x8(%ebp),%eax
  800729:	3b 45 fc             	cmp    -0x4(%ebp),%eax
  80072c:	77 24                	ja     800752 <free_locked+0x4f>
  80072e:	8b 45 fc             	mov    -0x4(%ebp),%eax
  800731:	8b 00                	mov    (%eax),%eax
  800733:	39 45 f8             	cmp    %eax,-0x8(%ebp)
  800736:	72 1a                	jb     800752 <free_locked+0x4f>
    for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
  800738:	8b 45 fc             	mov    -0x4(%ebp),%eax
  80073b:	8b 00                	mov    (%eax),%eax
  80073d:	89 45 fc             	mov    %eax,-0x4(%ebp)
  800740:	8b 45 f8             	mov    -0x8(%ebp),%eax
  800743:	3b 45 fc             	cmp    -0x4(%ebp),%eax
  800746:	76 d4                	jbe    80071c <free_locked+0x19>
  800748:	8b 45 fc             	mov    -0x4(%ebp),%eax
  80074b:	8b 00                	mov    (%eax),%eax
  80074d:	39 45 f8             	cmp    %eax,-0x8(%ebp)
  800750:	73 ca                	jae    80071c <free_locked+0x19>
              */
            break;
        }
    }

    if (bp->s.type == p->s.ptr->s.type && bp + bp->s.size == p->s.ptr)
  800752:	8b 45 f8             	mov    -0x8(%ebp),%eax
  800755:	8b 50 08             	mov    0x8(%eax),%edx
  800758:	8b 45 fc             	mov    -0x4(%ebp),%eax
  80075b:	8b 00                	mov    (%eax),%eax
  80075d:	8b 40 08             	mov    0x8(%eax),%eax
  800760:	39 c2                	cmp    %eax,%edx
  800762:	75 3d                	jne    8007a1 <free_locked+0x9e>
  800764:	8b 45 f8             	mov    -0x8(%ebp),%eax
  800767:	8b 40 04             	mov    0x4(%eax),%eax
  80076a:	c1 e0 06             	shl    $0x6,%eax
  80076d:	89 c2                	mov    %eax,%edx
  80076f:	8b 45 f8             	mov    -0x8(%ebp),%eax
  800772:	01 c2                	add    %eax,%edx
  800774:	8b 45 fc             	mov    -0x4(%ebp),%eax
  800777:	8b 00                	mov    (%eax),%eax
  800779:	39 c2                	cmp    %eax,%edx
  80077b:	75 24                	jne    8007a1 <free_locked+0x9e>
    {
        //p---bp---bp.s.ptr
        //类型一样，并且和后面的可以合并
        bp->s.size += p->s.ptr->s.size;
  80077d:	8b 45 f8             	mov    -0x8(%ebp),%eax
  800780:	8b 50 04             	mov    0x4(%eax),%edx
  800783:	8b 45 fc             	mov    -0x4(%ebp),%eax
  800786:	8b 00                	mov    (%eax),%eax
  800788:	8b 40 04             	mov    0x4(%eax),%eax
  80078b:	01 c2                	add    %eax,%edx
  80078d:	8b 45 f8             	mov    -0x8(%ebp),%eax
  800790:	89 50 04             	mov    %edx,0x4(%eax)
        bp->s.ptr = p->s.ptr->s.ptr;
  800793:	8b 45 fc             	mov    -0x4(%ebp),%eax
  800796:	8b 00                	mov    (%eax),%eax
  800798:	8b 10                	mov    (%eax),%edx
  80079a:	8b 45 f8             	mov    -0x8(%ebp),%eax
  80079d:	89 10                	mov    %edx,(%eax)
  80079f:	eb 0a                	jmp    8007ab <free_locked+0xa8>
    }
    else
    {
        //连接好
        bp->s.ptr = p->s.ptr;
  8007a1:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8007a4:	8b 10                	mov    (%eax),%edx
  8007a6:	8b 45 f8             	mov    -0x8(%ebp),%eax
  8007a9:	89 10                	mov    %edx,(%eax)
    }

    if (p->s.type == bp->s.type && p + p->s.size == bp)
  8007ab:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8007ae:	8b 50 08             	mov    0x8(%eax),%edx
  8007b1:	8b 45 f8             	mov    -0x8(%ebp),%eax
  8007b4:	8b 40 08             	mov    0x8(%eax),%eax
  8007b7:	39 c2                	cmp    %eax,%edx
  8007b9:	75 35                	jne    8007f0 <free_locked+0xed>
  8007bb:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8007be:	8b 40 04             	mov    0x4(%eax),%eax
  8007c1:	c1 e0 06             	shl    $0x6,%eax
  8007c4:	89 c2                	mov    %eax,%edx
  8007c6:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8007c9:	01 d0                	add    %edx,%eax
  8007cb:	39 45 f8             	cmp    %eax,-0x8(%ebp)
  8007ce:	75 20                	jne    8007f0 <free_locked+0xed>
    {
        //类型一样，并且和前面的可以合并
        p->s.size += bp->s.size;
  8007d0:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8007d3:	8b 50 04             	mov    0x4(%eax),%edx
  8007d6:	8b 45 f8             	mov    -0x8(%ebp),%eax
  8007d9:	8b 40 04             	mov    0x4(%eax),%eax
  8007dc:	01 c2                	add    %eax,%edx
  8007de:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8007e1:	89 50 04             	mov    %edx,0x4(%eax)
        p->s.ptr = bp->s.ptr;
  8007e4:	8b 45 f8             	mov    -0x8(%ebp),%eax
  8007e7:	8b 10                	mov    (%eax),%edx
  8007e9:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8007ec:	89 10                	mov    %edx,(%eax)
  8007ee:	eb 08                	jmp    8007f8 <free_locked+0xf5>
    }
    else
    {
        //重新连接好
        p->s.ptr = bp;
  8007f0:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8007f3:	8b 55 f8             	mov    -0x8(%ebp),%edx
  8007f6:	89 10                	mov    %edx,(%eax)
    }
    //下次可以分配的位置
    freep = p;
  8007f8:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8007fb:	a3 00 31 80 00       	mov    %eax,0x803100
}
  800800:	90                   	nop
  800801:	c9                   	leave  
  800802:	c3                   	ret    

00800803 <malloc>:

void *malloc(size_t size)
{
  800803:	55                   	push   %ebp
  800804:	89 e5                	mov    %esp,%ebp
  800806:	83 ec 18             	sub    $0x18,%esp
    void *ret;
    lock_malloc();
  800809:	e8 5e fc ff ff       	call   80046c <lock_malloc>
    ret = malloc_locked(size, 0);
  80080e:	83 ec 08             	sub    $0x8,%esp
  800811:	6a 00                	push   $0x0
  800813:	ff 75 08             	pushl  0x8(%ebp)
  800816:	e8 be fd ff ff       	call   8005d9 <malloc_locked>
  80081b:	83 c4 10             	add    $0x10,%esp
  80081e:	89 45 f4             	mov    %eax,-0xc(%ebp)
    unlock_malloc();
  800821:	e8 64 fc ff ff       	call   80048a <unlock_malloc>
    return ret;
  800826:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
  800829:	c9                   	leave  
  80082a:	c3                   	ret    

0080082b <shmem_malloc>:

void *shmem_malloc(size_t size)
{
  80082b:	55                   	push   %ebp
  80082c:	89 e5                	mov    %esp,%ebp
  80082e:	83 ec 18             	sub    $0x18,%esp
    void *ret;
    lock_malloc();
  800831:	e8 36 fc ff ff       	call   80046c <lock_malloc>
    ret = malloc_locked(size, 1);
  800836:	83 ec 08             	sub    $0x8,%esp
  800839:	6a 01                	push   $0x1
  80083b:	ff 75 08             	pushl  0x8(%ebp)
  80083e:	e8 96 fd ff ff       	call   8005d9 <malloc_locked>
  800843:	83 c4 10             	add    $0x10,%esp
  800846:	89 45 f4             	mov    %eax,-0xc(%ebp)
    //cprintf("shmem_malloc *ret =%x\n",*((int*)(ret)));
    unlock_malloc();
  800849:	e8 3c fc ff ff       	call   80048a <unlock_malloc>
    return ret;
  80084e:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
  800851:	c9                   	leave  
  800852:	c3                   	ret    

00800853 <free>:

void free(void *ap)
{
  800853:	55                   	push   %ebp
  800854:	89 e5                	mov    %esp,%ebp
  800856:	83 ec 08             	sub    $0x8,%esp
    lock_malloc();
  800859:	e8 0e fc ff ff       	call   80046c <lock_malloc>
    free_locked(ap);
  80085e:	83 ec 0c             	sub    $0xc,%esp
  800861:	ff 75 08             	pushl  0x8(%ebp)
  800864:	e8 9a fe ff ff       	call   800703 <free_locked>
  800869:	83 c4 10             	add    $0x10,%esp
    unlock_malloc();
  80086c:	e8 19 fc ff ff       	call   80048a <unlock_malloc>
}
  800871:	90                   	nop
  800872:	c9                   	leave  
  800873:	c3                   	ret    

00800874 <mount>:
#include "mount.h"
#include "syscall.h"

int mount(const char *source, const char *target, const char *filesystemtype, const void *data)
{
  800874:	55                   	push   %ebp
  800875:	89 e5                	mov    %esp,%ebp
  800877:	83 ec 08             	sub    $0x8,%esp
	return sys_mount(source, target, filesystemtype, data);
  80087a:	ff 75 14             	pushl  0x14(%ebp)
  80087d:	ff 75 10             	pushl  0x10(%ebp)
  800880:	ff 75 0c             	pushl  0xc(%ebp)
  800883:	ff 75 08             	pushl  0x8(%ebp)
  800886:	e8 17 06 00 00       	call   800ea2 <sys_mount>
  80088b:	83 c4 10             	add    $0x10,%esp
}
  80088e:	c9                   	leave  
  80088f:	c3                   	ret    

00800890 <umount>:

int umount(const char *target)
{
  800890:	55                   	push   %ebp
  800891:	89 e5                	mov    %esp,%ebp
  800893:	83 ec 08             	sub    $0x8,%esp
	return sys_umount(target);
  800896:	83 ec 0c             	sub    $0xc,%esp
  800899:	ff 75 08             	pushl  0x8(%ebp)
  80089c:	e8 1f 06 00 00       	call   800ec0 <sys_umount>
  8008a1:	83 c4 10             	add    $0x10,%esp
}
  8008a4:	c9                   	leave  
  8008a5:	c3                   	ret    

008008a6 <__panic>:
#include "stdio.h"
#include "ulib.h"
#include "error.h"

void __panic(const char *file, int line, const char *fmt, ...)
{
  8008a6:	55                   	push   %ebp
  8008a7:	89 e5                	mov    %esp,%ebp
  8008a9:	83 ec 18             	sub    $0x18,%esp
    // print the 'message'
    va_list ap;
    va_start(ap, fmt);
  8008ac:	8d 45 14             	lea    0x14(%ebp),%eax
  8008af:	89 45 f4             	mov    %eax,-0xc(%ebp)
    cprintf("user panic at %s:%d:\n    ", file, line);
  8008b2:	83 ec 04             	sub    $0x4,%esp
  8008b5:	ff 75 0c             	pushl  0xc(%ebp)
  8008b8:	ff 75 08             	pushl  0x8(%ebp)
  8008bb:	68 b6 28 80 00       	push   $0x8028b6
  8008c0:	e8 bd 01 00 00       	call   800a82 <cprintf>
  8008c5:	83 c4 10             	add    $0x10,%esp
    vcprintf(fmt, ap);
  8008c8:	8b 45 f4             	mov    -0xc(%ebp),%eax
  8008cb:	83 ec 08             	sub    $0x8,%esp
  8008ce:	50                   	push   %eax
  8008cf:	ff 75 10             	pushl  0x10(%ebp)
  8008d2:	e8 7a 01 00 00       	call   800a51 <vcprintf>
  8008d7:	83 c4 10             	add    $0x10,%esp
    cprintf("\n");
  8008da:	83 ec 0c             	sub    $0xc,%esp
  8008dd:	68 d0 28 80 00       	push   $0x8028d0
  8008e2:	e8 9b 01 00 00       	call   800a82 <cprintf>
  8008e7:	83 c4 10             	add    $0x10,%esp
    va_end(ap);
    exit(-E_PANIC);
  8008ea:	83 ec 0c             	sub    $0xc,%esp
  8008ed:	6a f6                	push   $0xfffffff6
  8008ef:	e8 f5 0a 00 00       	call   8013e9 <exit>

008008f4 <__warn>:
}

void __warn(const char *file, int line, const char *fmt, ...)
{
  8008f4:	55                   	push   %ebp
  8008f5:	89 e5                	mov    %esp,%ebp
  8008f7:	83 ec 18             	sub    $0x18,%esp
    va_list ap;
    va_start(ap, fmt);
  8008fa:	8d 45 14             	lea    0x14(%ebp),%eax
  8008fd:	89 45 f4             	mov    %eax,-0xc(%ebp)
    cprintf("user warning at %s:%d:\n    ", file, line);
  800900:	83 ec 04             	sub    $0x4,%esp
  800903:	ff 75 0c             	pushl  0xc(%ebp)
  800906:	ff 75 08             	pushl  0x8(%ebp)
  800909:	68 d2 28 80 00       	push   $0x8028d2
  80090e:	e8 6f 01 00 00       	call   800a82 <cprintf>
  800913:	83 c4 10             	add    $0x10,%esp
    vcprintf(fmt, ap);
  800916:	8b 45 f4             	mov    -0xc(%ebp),%eax
  800919:	83 ec 08             	sub    $0x8,%esp
  80091c:	50                   	push   %eax
  80091d:	ff 75 10             	pushl  0x10(%ebp)
  800920:	e8 2c 01 00 00       	call   800a51 <vcprintf>
  800925:	83 c4 10             	add    $0x10,%esp
    cprintf("\n");
  800928:	83 ec 0c             	sub    $0xc,%esp
  80092b:	68 d0 28 80 00       	push   $0x8028d0
  800930:	e8 4d 01 00 00       	call   800a82 <cprintf>
  800935:	83 c4 10             	add    $0x10,%esp
    va_end(ap);
}
  800938:	90                   	nop
  800939:	c9                   	leave  
  80093a:	c3                   	ret    

0080093b <sig_restorer>:
// 用户层在 signal handle 处理完之后，会来到这里
// 这里需要找到进入 hanlde 之前的堆栈地址，那里记录了处理 signal 之前的旧的 tf 中断桢结构
// 通过旧的 tf 中断桢，重新进入内核态，再次返回时，用户进程可以返回到最开始进入内核态时的地址，
// 这样 signal 是异步处理的，用户代码上没有连续性，也感觉不到 signal 的调用时机
void sig_restorer(int sign)
{
  80093b:	55                   	push   %ebp
  80093c:	89 e5                	mov    %esp,%ebp
  80093e:	83 ec 18             	sub    $0x18,%esp
#endif
}

static inline uint32_t read_ebp(void)
{
    uint32_t ebp = 0;
  800941:	c7 45 f0 00 00 00 00 	movl   $0x0,-0x10(%ebp)
#if ASM_NO_64
    asm volatile ("movl %%ebp, %0" : "=r" (ebp));
  800948:	89 e8                	mov    %ebp,%eax
  80094a:	89 45 f0             	mov    %eax,-0x10(%ebp)
#endif
    return ebp;
  80094d:	8b 45 f0             	mov    -0x10(%ebp),%eax
    uint32_t oldesp = read_ebp();
  800950:	89 45 f4             	mov    %eax,-0xc(%ebp)
    cprintf("signal restorer by %d\n", sys_getpid());
  800953:	e8 d4 02 00 00       	call   800c2c <sys_getpid>
  800958:	83 ec 08             	sub    $0x8,%esp
  80095b:	50                   	push   %eax
  80095c:	68 ee 28 80 00       	push   $0x8028ee
  800961:	e8 1c 01 00 00       	call   800a82 <cprintf>
  800966:	83 c4 10             	add    $0x10,%esp
    sys_sigreturn(oldesp);
  800969:	83 ec 0c             	sub    $0xc,%esp
  80096c:	ff 75 f4             	pushl  -0xc(%ebp)
  80096f:	e8 c4 05 00 00       	call   800f38 <sys_sigreturn>
  800974:	83 c4 10             	add    $0x10,%esp
}
  800977:	90                   	nop
  800978:	c9                   	leave  
  800979:	c3                   	ret    

0080097a <signal>:

int signal(int sign, sighandler_t handler)
{
  80097a:	55                   	push   %ebp
  80097b:	89 e5                	mov    %esp,%ebp
  80097d:	83 ec 28             	sub    $0x28,%esp
    struct sigaction act = { handler, NULL, 1 << (sign - 1), 0, sig_restorer };
  800980:	8b 45 0c             	mov    0xc(%ebp),%eax
  800983:	89 45 e0             	mov    %eax,-0x20(%ebp)
  800986:	c7 45 e4 00 00 00 00 	movl   $0x0,-0x1c(%ebp)
  80098d:	8b 45 08             	mov    0x8(%ebp),%eax
  800990:	48                   	dec    %eax
  800991:	ba 01 00 00 00       	mov    $0x1,%edx
  800996:	88 c1                	mov    %al,%cl
  800998:	d3 e2                	shl    %cl,%edx
  80099a:	89 d0                	mov    %edx,%eax
  80099c:	99                   	cltd   
  80099d:	89 45 e8             	mov    %eax,-0x18(%ebp)
  8009a0:	89 55 ec             	mov    %edx,-0x14(%ebp)
  8009a3:	c7 45 f0 00 00 00 00 	movl   $0x0,-0x10(%ebp)
  8009aa:	c7 45 f4 3b 09 80 00 	movl   $0x80093b,-0xc(%ebp)
    return sys_sigaction(sign, &act, NULL);
  8009b1:	83 ec 04             	sub    $0x4,%esp
  8009b4:	6a 00                	push   $0x0
  8009b6:	8d 45 e0             	lea    -0x20(%ebp),%eax
  8009b9:	50                   	push   %eax
  8009ba:	ff 75 08             	pushl  0x8(%ebp)
  8009bd:	e8 13 05 00 00       	call   800ed5 <sys_sigaction>
  8009c2:	83 c4 10             	add    $0x10,%esp
}
  8009c5:	c9                   	leave  
  8009c6:	c3                   	ret    

008009c7 <sigaction>:

int sigaction(int sign, struct sigaction *act)
{
  8009c7:	55                   	push   %ebp
  8009c8:	89 e5                	mov    %esp,%ebp
  8009ca:	83 ec 08             	sub    $0x8,%esp
    return sys_sigaction(sign, act, NULL);
  8009cd:	83 ec 04             	sub    $0x4,%esp
  8009d0:	6a 00                	push   $0x0
  8009d2:	ff 75 0c             	pushl  0xc(%ebp)
  8009d5:	ff 75 08             	pushl  0x8(%ebp)
  8009d8:	e8 f8 04 00 00       	call   800ed5 <sys_sigaction>
  8009dd:	83 c4 10             	add    $0x10,%esp
}
  8009e0:	c9                   	leave  
  8009e1:	c3                   	ret    

008009e2 <tkill>:

int tkill(int pid, int sign)
{
  8009e2:	55                   	push   %ebp
  8009e3:	89 e5                	mov    %esp,%ebp
  8009e5:	83 ec 08             	sub    $0x8,%esp
	return sys_sigtkill(pid, sign);
  8009e8:	83 ec 08             	sub    $0x8,%esp
  8009eb:	ff 75 0c             	pushl  0xc(%ebp)
  8009ee:	ff 75 08             	pushl  0x8(%ebp)
  8009f1:	e8 fa 04 00 00       	call   800ef0 <sys_sigtkill>
  8009f6:	83 c4 10             	add    $0x10,%esp
}
  8009f9:	c9                   	leave  
  8009fa:	c3                   	ret    

008009fb <sigprocmask>:

int sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
  8009fb:	55                   	push   %ebp
  8009fc:	89 e5                	mov    %esp,%ebp
  8009fe:	83 ec 08             	sub    $0x8,%esp
	return sys_sigprocmask(how, set, old);
  800a01:	83 ec 04             	sub    $0x4,%esp
  800a04:	ff 75 10             	pushl  0x10(%ebp)
  800a07:	ff 75 0c             	pushl  0xc(%ebp)
  800a0a:	ff 75 08             	pushl  0x8(%ebp)
  800a0d:	e8 f6 04 00 00       	call   800f08 <sys_sigprocmask>
  800a12:	83 c4 10             	add    $0x10,%esp
}
  800a15:	c9                   	leave  
  800a16:	c3                   	ret    

00800a17 <sigsuspend>:

int sigsuspend(uint32_t mask)
{
  800a17:	55                   	push   %ebp
  800a18:	89 e5                	mov    %esp,%ebp
  800a1a:	83 ec 08             	sub    $0x8,%esp
	return sys_sigsuspend(mask);
  800a1d:	83 ec 0c             	sub    $0xc,%esp
  800a20:	ff 75 08             	pushl  0x8(%ebp)
  800a23:	e8 fb 04 00 00       	call   800f23 <sys_sigsuspend>
  800a28:	83 c4 10             	add    $0x10,%esp
}
  800a2b:	c9                   	leave  
  800a2c:	c3                   	ret    

00800a2d <cputch>:
/* *
 * cputch - writes a single character @c to stdout, and it will
 * increace the value of counter pointed by @cnt.
 * */
static void cputch(int c, int *cnt)
{
  800a2d:	55                   	push   %ebp
  800a2e:	89 e5                	mov    %esp,%ebp
  800a30:	83 ec 08             	sub    $0x8,%esp
    sys_putc(c);
  800a33:	83 ec 0c             	sub    $0xc,%esp
  800a36:	ff 75 08             	pushl  0x8(%ebp)
  800a39:	e8 fd 01 00 00       	call   800c3b <sys_putc>
  800a3e:	83 c4 10             	add    $0x10,%esp
    (*cnt) ++;
  800a41:	8b 45 0c             	mov    0xc(%ebp),%eax
  800a44:	8b 00                	mov    (%eax),%eax
  800a46:	8d 50 01             	lea    0x1(%eax),%edx
  800a49:	8b 45 0c             	mov    0xc(%ebp),%eax
  800a4c:	89 10                	mov    %edx,(%eax)
}
  800a4e:	90                   	nop
  800a4f:	c9                   	leave  
  800a50:	c3                   	ret    

00800a51 <vcprintf>:
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want cprintf() instead.
 * */
int vcprintf(const char *fmt, va_list ap)
{
  800a51:	55                   	push   %ebp
  800a52:	89 e5                	mov    %esp,%ebp
  800a54:	83 ec 18             	sub    $0x18,%esp
    int cnt = 0;
  800a57:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
    vprintfmt((void *)cputch, NO_FD, &cnt, fmt, ap);
  800a5e:	83 ec 0c             	sub    $0xc,%esp
  800a61:	ff 75 0c             	pushl  0xc(%ebp)
  800a64:	ff 75 08             	pushl  0x8(%ebp)
  800a67:	8d 45 f4             	lea    -0xc(%ebp),%eax
  800a6a:	50                   	push   %eax
  800a6b:	68 d9 6a ff ff       	push   $0xffff6ad9
  800a70:	68 2d 0a 80 00       	push   $0x800a2d
  800a75:	e8 39 11 00 00       	call   801bb3 <vprintfmt>
  800a7a:	83 c4 20             	add    $0x20,%esp
    return cnt;
  800a7d:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
  800a80:	c9                   	leave  
  800a81:	c3                   	ret    

00800a82 <cprintf>:
 *
 * The return value is the number of characters which would be
 * written to stdout.
 * */
int cprintf(const char *fmt, ...)
{
  800a82:	55                   	push   %ebp
  800a83:	89 e5                	mov    %esp,%ebp
  800a85:	83 ec 18             	sub    $0x18,%esp
    va_list ap;

    va_start(ap, fmt);
  800a88:	8d 45 0c             	lea    0xc(%ebp),%eax
  800a8b:	89 45 f0             	mov    %eax,-0x10(%ebp)
    int cnt = vcprintf(fmt, ap);
  800a8e:	8b 45 f0             	mov    -0x10(%ebp),%eax
  800a91:	83 ec 08             	sub    $0x8,%esp
  800a94:	50                   	push   %eax
  800a95:	ff 75 08             	pushl  0x8(%ebp)
  800a98:	e8 b4 ff ff ff       	call   800a51 <vcprintf>
  800a9d:	83 c4 10             	add    $0x10,%esp
  800aa0:	89 45 f4             	mov    %eax,-0xc(%ebp)
    va_end(ap);

    return cnt;
  800aa3:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
  800aa6:	c9                   	leave  
  800aa7:	c3                   	ret    

00800aa8 <cputs>:
/* *
 * cputs- writes the string pointed by @str to stdout and
 * appends a newline character.
 * */
int cputs(const char *str)
{
  800aa8:	55                   	push   %ebp
  800aa9:	89 e5                	mov    %esp,%ebp
  800aab:	83 ec 18             	sub    $0x18,%esp
    int cnt = 0;
  800aae:	c7 45 f0 00 00 00 00 	movl   $0x0,-0x10(%ebp)
    char c;
    while ((c = *str ++) != '\0')
  800ab5:	eb 14                	jmp    800acb <cputs+0x23>
    {
        cputch(c, &cnt);
  800ab7:	0f be 45 f7          	movsbl -0x9(%ebp),%eax
  800abb:	83 ec 08             	sub    $0x8,%esp
  800abe:	8d 55 f0             	lea    -0x10(%ebp),%edx
  800ac1:	52                   	push   %edx
  800ac2:	50                   	push   %eax
  800ac3:	e8 65 ff ff ff       	call   800a2d <cputch>
  800ac8:	83 c4 10             	add    $0x10,%esp
    while ((c = *str ++) != '\0')
  800acb:	8b 45 08             	mov    0x8(%ebp),%eax
  800ace:	8d 50 01             	lea    0x1(%eax),%edx
  800ad1:	89 55 08             	mov    %edx,0x8(%ebp)
  800ad4:	8a 00                	mov    (%eax),%al
  800ad6:	88 45 f7             	mov    %al,-0x9(%ebp)
  800ad9:	80 7d f7 00          	cmpb   $0x0,-0x9(%ebp)
  800add:	75 d8                	jne    800ab7 <cputs+0xf>
    }
    cputch('\n', &cnt);
  800adf:	83 ec 08             	sub    $0x8,%esp
  800ae2:	8d 45 f0             	lea    -0x10(%ebp),%eax
  800ae5:	50                   	push   %eax
  800ae6:	6a 0a                	push   $0xa
  800ae8:	e8 40 ff ff ff       	call   800a2d <cputch>
  800aed:	83 c4 10             	add    $0x10,%esp
    return cnt;
  800af0:	8b 45 f0             	mov    -0x10(%ebp),%eax
}
  800af3:	c9                   	leave  
  800af4:	c3                   	ret    

00800af5 <fputch>:

static void fputch(char c, int *cnt, int fd)
{
  800af5:	55                   	push   %ebp
  800af6:	89 e5                	mov    %esp,%ebp
  800af8:	83 ec 18             	sub    $0x18,%esp
  800afb:	8b 45 08             	mov    0x8(%ebp),%eax
  800afe:	88 45 f4             	mov    %al,-0xc(%ebp)
    write(fd, &c, sizeof(char));
  800b01:	83 ec 04             	sub    $0x4,%esp
  800b04:	6a 01                	push   $0x1
  800b06:	8d 45 f4             	lea    -0xc(%ebp),%eax
  800b09:	50                   	push   %eax
  800b0a:	ff 75 10             	pushl  0x10(%ebp)
  800b0d:	e8 f3 f6 ff ff       	call   800205 <write>
  800b12:	83 c4 10             	add    $0x10,%esp
    (*cnt) ++;
  800b15:	8b 45 0c             	mov    0xc(%ebp),%eax
  800b18:	8b 00                	mov    (%eax),%eax
  800b1a:	8d 50 01             	lea    0x1(%eax),%edx
  800b1d:	8b 45 0c             	mov    0xc(%ebp),%eax
  800b20:	89 10                	mov    %edx,(%eax)
}
  800b22:	90                   	nop
  800b23:	c9                   	leave  
  800b24:	c3                   	ret    

00800b25 <vfprintf>:

int vfprintf(int fd, const char *fmt, va_list ap)
{
  800b25:	55                   	push   %ebp
  800b26:	89 e5                	mov    %esp,%ebp
  800b28:	83 ec 18             	sub    $0x18,%esp
    int cnt = 0;
  800b2b:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
    vprintfmt((void*)fputch, fd, &cnt, fmt, ap);
  800b32:	83 ec 0c             	sub    $0xc,%esp
  800b35:	ff 75 10             	pushl  0x10(%ebp)
  800b38:	ff 75 0c             	pushl  0xc(%ebp)
  800b3b:	8d 45 f4             	lea    -0xc(%ebp),%eax
  800b3e:	50                   	push   %eax
  800b3f:	ff 75 08             	pushl  0x8(%ebp)
  800b42:	68 f5 0a 80 00       	push   $0x800af5
  800b47:	e8 67 10 00 00       	call   801bb3 <vprintfmt>
  800b4c:	83 c4 20             	add    $0x20,%esp
    return cnt;
  800b4f:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
  800b52:	c9                   	leave  
  800b53:	c3                   	ret    

00800b54 <fprintf>:

int fprintf(int fd, const char *fmt, ...)
{
  800b54:	55                   	push   %ebp
  800b55:	89 e5                	mov    %esp,%ebp
  800b57:	83 ec 18             	sub    $0x18,%esp
    va_list ap;

    va_start(ap, fmt);
  800b5a:	8d 45 10             	lea    0x10(%ebp),%eax
  800b5d:	89 45 f0             	mov    %eax,-0x10(%ebp)
    int cnt = vfprintf(fd, fmt, ap);
  800b60:	8b 45 f0             	mov    -0x10(%ebp),%eax
  800b63:	83 ec 04             	sub    $0x4,%esp
  800b66:	50                   	push   %eax
  800b67:	ff 75 0c             	pushl  0xc(%ebp)
  800b6a:	ff 75 08             	pushl  0x8(%ebp)
  800b6d:	e8 b3 ff ff ff       	call   800b25 <vfprintf>
  800b72:	83 c4 10             	add    $0x10,%esp
  800b75:	89 45 f4             	mov    %eax,-0xc(%ebp)
    va_end(ap);

    return cnt;
  800b78:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
  800b7b:	c9                   	leave  
  800b7c:	c3                   	ret    

00800b7d <syscall>:

#define MAX_ARGS            5

// 通过陷阱门来实现系统调用
static inline int syscall(int num, ...)
{
  800b7d:	55                   	push   %ebp
  800b7e:	89 e5                	mov    %esp,%ebp
  800b80:	57                   	push   %edi
  800b81:	56                   	push   %esi
  800b82:	53                   	push   %ebx
  800b83:	83 ec 20             	sub    $0x20,%esp
    va_list ap;
    va_start(ap, num);
  800b86:	8d 45 0c             	lea    0xc(%ebp),%eax
  800b89:	89 45 e8             	mov    %eax,-0x18(%ebp)
    uint32_t a[MAX_ARGS];
    int i, ret;
    for (i = 0; i < MAX_ARGS; i ++)
  800b8c:	c7 45 f0 00 00 00 00 	movl   $0x0,-0x10(%ebp)
  800b93:	eb 15                	jmp    800baa <syscall+0x2d>
    {
        a[i] = va_arg(ap, uint32_t);
  800b95:	8b 45 e8             	mov    -0x18(%ebp),%eax
  800b98:	8d 50 04             	lea    0x4(%eax),%edx
  800b9b:	89 55 e8             	mov    %edx,-0x18(%ebp)
  800b9e:	8b 10                	mov    (%eax),%edx
  800ba0:	8b 45 f0             	mov    -0x10(%ebp),%eax
  800ba3:	89 54 85 d4          	mov    %edx,-0x2c(%ebp,%eax,4)
    for (i = 0; i < MAX_ARGS; i ++)
  800ba7:	ff 45 f0             	incl   -0x10(%ebp)
  800baa:	83 7d f0 04          	cmpl   $0x4,-0x10(%ebp)
  800bae:	7e e5                	jle    800b95 <syscall+0x18>
    asm volatile (
        "int %1;"
        : "=a" (ret)
        : "i" (T_SYSCALL),
          "a" (num),
          "d" (a[0]),
  800bb0:	8b 55 d4             	mov    -0x2c(%ebp),%edx
          "c" (a[1]),
  800bb3:	8b 4d d8             	mov    -0x28(%ebp),%ecx
          "b" (a[2]),
  800bb6:	8b 5d dc             	mov    -0x24(%ebp),%ebx
          "D" (a[3]),
  800bb9:	8b 7d e0             	mov    -0x20(%ebp),%edi
          "S" (a[4])
  800bbc:	8b 75 e4             	mov    -0x1c(%ebp),%esi
    asm volatile (
  800bbf:	8b 45 08             	mov    0x8(%ebp),%eax
  800bc2:	cd 80                	int    $0x80
  800bc4:	89 45 ec             	mov    %eax,-0x14(%ebp)
        : "cc", "memory");
    return ret;
  800bc7:	8b 45 ec             	mov    -0x14(%ebp),%eax
}
  800bca:	83 c4 20             	add    $0x20,%esp
  800bcd:	5b                   	pop    %ebx
  800bce:	5e                   	pop    %esi
  800bcf:	5f                   	pop    %edi
  800bd0:	5d                   	pop    %ebp
  800bd1:	c3                   	ret    

00800bd2 <sys_exit>:
//                  : "cc", "memory");
//    return ret;
//}

int sys_exit(int error_code)
{
  800bd2:	55                   	push   %ebp
  800bd3:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_exit, error_code);
  800bd5:	ff 75 08             	pushl  0x8(%ebp)
  800bd8:	6a 01                	push   $0x1
  800bda:	e8 9e ff ff ff       	call   800b7d <syscall>
  800bdf:	83 c4 08             	add    $0x8,%esp
}
  800be2:	c9                   	leave  
  800be3:	c3                   	ret    

00800be4 <sys_fork>:

int sys_fork(char *name)
{
  800be4:	55                   	push   %ebp
  800be5:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_fork, name);
  800be7:	ff 75 08             	pushl  0x8(%ebp)
  800bea:	6a 02                	push   $0x2
  800bec:	e8 8c ff ff ff       	call   800b7d <syscall>
  800bf1:	83 c4 08             	add    $0x8,%esp
}
  800bf4:	c9                   	leave  
  800bf5:	c3                   	ret    

00800bf6 <sys_wait>:

int sys_wait(int pid, int *store)
{
  800bf6:	55                   	push   %ebp
  800bf7:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_wait, pid, store);
  800bf9:	ff 75 0c             	pushl  0xc(%ebp)
  800bfc:	ff 75 08             	pushl  0x8(%ebp)
  800bff:	6a 03                	push   $0x3
  800c01:	e8 77 ff ff ff       	call   800b7d <syscall>
  800c06:	83 c4 0c             	add    $0xc,%esp
}
  800c09:	c9                   	leave  
  800c0a:	c3                   	ret    

00800c0b <sys_yield>:

int sys_yield(void)
{
  800c0b:	55                   	push   %ebp
  800c0c:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_yield);
  800c0e:	6a 0a                	push   $0xa
  800c10:	e8 68 ff ff ff       	call   800b7d <syscall>
  800c15:	83 c4 04             	add    $0x4,%esp
}
  800c18:	c9                   	leave  
  800c19:	c3                   	ret    

00800c1a <sys_kill>:

int sys_kill(int pid)
{
  800c1a:	55                   	push   %ebp
  800c1b:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_kill, pid);
  800c1d:	ff 75 08             	pushl  0x8(%ebp)
  800c20:	6a 0c                	push   $0xc
  800c22:	e8 56 ff ff ff       	call   800b7d <syscall>
  800c27:	83 c4 08             	add    $0x8,%esp
}
  800c2a:	c9                   	leave  
  800c2b:	c3                   	ret    

00800c2c <sys_getpid>:

int sys_getpid(void)
{
  800c2c:	55                   	push   %ebp
  800c2d:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_getpid);
  800c2f:	6a 12                	push   $0x12
  800c31:	e8 47 ff ff ff       	call   800b7d <syscall>
  800c36:	83 c4 04             	add    $0x4,%esp
}
  800c39:	c9                   	leave  
  800c3a:	c3                   	ret    

00800c3b <sys_putc>:

int sys_putc(int c)
{
  800c3b:	55                   	push   %ebp
  800c3c:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_putc, c);
  800c3e:	ff 75 08             	pushl  0x8(%ebp)
  800c41:	6a 1e                	push   $0x1e
  800c43:	e8 35 ff ff ff       	call   800b7d <syscall>
  800c48:	83 c4 08             	add    $0x8,%esp
}
  800c4b:	c9                   	leave  
  800c4c:	c3                   	ret    

00800c4d <sys_shmem>:

int sys_shmem(uintptr_t *addr_store, size_t len, uint32_t mmap_flags)
{
  800c4d:	55                   	push   %ebp
  800c4e:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_shmem, (int)addr_store, len, mmap_flags, 0, 0);
  800c50:	8b 45 08             	mov    0x8(%ebp),%eax
  800c53:	6a 00                	push   $0x0
  800c55:	6a 00                	push   $0x0
  800c57:	ff 75 10             	pushl  0x10(%ebp)
  800c5a:	ff 75 0c             	pushl  0xc(%ebp)
  800c5d:	50                   	push   %eax
  800c5e:	6a 16                	push   $0x16
  800c60:	e8 18 ff ff ff       	call   800b7d <syscall>
  800c65:	83 c4 18             	add    $0x18,%esp
}
  800c68:	c9                   	leave  
  800c69:	c3                   	ret    

00800c6a <sys_pgdir>:

int sys_pgdir(void)
{
  800c6a:	55                   	push   %ebp
  800c6b:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_pgdir);
  800c6d:	6a 1f                	push   $0x1f
  800c6f:	e8 09 ff ff ff       	call   800b7d <syscall>
  800c74:	83 c4 04             	add    $0x4,%esp
}
  800c77:	c9                   	leave  
  800c78:	c3                   	ret    

00800c79 <sys_pvm>:

int sys_pvm(void)
{
  800c79:	55                   	push   %ebp
  800c7a:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_pvm);
  800c7c:	6a 21                	push   $0x21
  800c7e:	e8 fa fe ff ff       	call   800b7d <syscall>
  800c83:	83 c4 04             	add    $0x4,%esp
}
  800c86:	c9                   	leave  
  800c87:	c3                   	ret    

00800c88 <sys_pvfs>:

int sys_pvfs(void)
{
  800c88:	55                   	push   %ebp
  800c89:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_pvfs);
  800c8b:	6a 20                	push   $0x20
  800c8d:	e8 eb fe ff ff       	call   800b7d <syscall>
  800c92:	83 c4 04             	add    $0x4,%esp
}
  800c95:	c9                   	leave  
  800c96:	c3                   	ret    

00800c97 <sys_set_priority>:

void sys_set_priority(uint32_t priority)
{
  800c97:	55                   	push   %ebp
  800c98:	89 e5                	mov    %esp,%ebp
    syscall(SYS_set_priority, priority);
  800c9a:	ff 75 08             	pushl  0x8(%ebp)
  800c9d:	68 ff 00 00 00       	push   $0xff
  800ca2:	e8 d6 fe ff ff       	call   800b7d <syscall>
  800ca7:	83 c4 08             	add    $0x8,%esp
}
  800caa:	90                   	nop
  800cab:	c9                   	leave  
  800cac:	c3                   	ret    

00800cad <sys_sleep>:

int sys_sleep(unsigned int time)
{
  800cad:	55                   	push   %ebp
  800cae:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sleep, time);
  800cb0:	ff 75 08             	pushl  0x8(%ebp)
  800cb3:	6a 0b                	push   $0xb
  800cb5:	e8 c3 fe ff ff       	call   800b7d <syscall>
  800cba:	83 c4 08             	add    $0x8,%esp
}
  800cbd:	c9                   	leave  
  800cbe:	c3                   	ret    

00800cbf <sys_gettime>:

size_t sys_gettime(void)
{
  800cbf:	55                   	push   %ebp
  800cc0:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_gettime);
  800cc2:	6a 11                	push   $0x11
  800cc4:	e8 b4 fe ff ff       	call   800b7d <syscall>
  800cc9:	83 c4 04             	add    $0x4,%esp
}
  800ccc:	c9                   	leave  
  800ccd:	c3                   	ret    

00800cce <sys_brk>:

int sys_brk(uintptr_t * brk_store)
{
  800cce:	55                   	push   %ebp
  800ccf:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_brk, brk_store);
  800cd1:	ff 75 08             	pushl  0x8(%ebp)
  800cd4:	6a 13                	push   $0x13
  800cd6:	e8 a2 fe ff ff       	call   800b7d <syscall>
  800cdb:	83 c4 08             	add    $0x8,%esp
}
  800cde:	c9                   	leave  
  800cdf:	c3                   	ret    

00800ce0 <sys_mmap>:

int sys_mmap(uintptr_t * addr_store, size_t len, uint32_t mmap_flags)
{
  800ce0:	55                   	push   %ebp
  800ce1:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_mmap, addr_store, len, mmap_flags);
  800ce3:	ff 75 10             	pushl  0x10(%ebp)
  800ce6:	ff 75 0c             	pushl  0xc(%ebp)
  800ce9:	ff 75 08             	pushl  0x8(%ebp)
  800cec:	6a 14                	push   $0x14
  800cee:	e8 8a fe ff ff       	call   800b7d <syscall>
  800cf3:	83 c4 10             	add    $0x10,%esp
}
  800cf6:	c9                   	leave  
  800cf7:	c3                   	ret    

00800cf8 <sys_munmap>:

int sys_munmap(uintptr_t addr, size_t len)
{
  800cf8:	55                   	push   %ebp
  800cf9:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_munmap, addr, len);
  800cfb:	ff 75 0c             	pushl  0xc(%ebp)
  800cfe:	ff 75 08             	pushl  0x8(%ebp)
  800d01:	6a 15                	push   $0x15
  800d03:	e8 75 fe ff ff       	call   800b7d <syscall>
  800d08:	83 c4 0c             	add    $0xc,%esp
}
  800d0b:	c9                   	leave  
  800d0c:	c3                   	ret    

00800d0d <sys_exec>:

int sys_exec(const char *name, int argc, const char **argv)
{
  800d0d:	55                   	push   %ebp
  800d0e:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_exec, name, argc, argv);
  800d10:	ff 75 10             	pushl  0x10(%ebp)
  800d13:	ff 75 0c             	pushl  0xc(%ebp)
  800d16:	ff 75 08             	pushl  0x8(%ebp)
  800d19:	6a 04                	push   $0x4
  800d1b:	e8 5d fe ff ff       	call   800b7d <syscall>
  800d20:	83 c4 10             	add    $0x10,%esp
}
  800d23:	c9                   	leave  
  800d24:	c3                   	ret    

00800d25 <sys_open>:

int sys_open(const char *path, uint32_t open_flags, uint32_t arg2)
{
  800d25:	55                   	push   %ebp
  800d26:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_open, path, open_flags, arg2);
  800d28:	ff 75 10             	pushl  0x10(%ebp)
  800d2b:	ff 75 0c             	pushl  0xc(%ebp)
  800d2e:	ff 75 08             	pushl  0x8(%ebp)
  800d31:	6a 64                	push   $0x64
  800d33:	e8 45 fe ff ff       	call   800b7d <syscall>
  800d38:	83 c4 10             	add    $0x10,%esp
}
  800d3b:	c9                   	leave  
  800d3c:	c3                   	ret    

00800d3d <sys_close>:

int sys_close(int fd)
{
  800d3d:	55                   	push   %ebp
  800d3e:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_close, fd);
  800d40:	ff 75 08             	pushl  0x8(%ebp)
  800d43:	6a 65                	push   $0x65
  800d45:	e8 33 fe ff ff       	call   800b7d <syscall>
  800d4a:	83 c4 08             	add    $0x8,%esp
}
  800d4d:	c9                   	leave  
  800d4e:	c3                   	ret    

00800d4f <sys_read>:

int sys_read(int fd, void *base, size_t len)
{
  800d4f:	55                   	push   %ebp
  800d50:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_read, fd, base, len);
  800d52:	ff 75 10             	pushl  0x10(%ebp)
  800d55:	ff 75 0c             	pushl  0xc(%ebp)
  800d58:	ff 75 08             	pushl  0x8(%ebp)
  800d5b:	6a 66                	push   $0x66
  800d5d:	e8 1b fe ff ff       	call   800b7d <syscall>
  800d62:	83 c4 10             	add    $0x10,%esp
}
  800d65:	c9                   	leave  
  800d66:	c3                   	ret    

00800d67 <sys_write>:

int sys_write(int fd, void *base, size_t len)
{
  800d67:	55                   	push   %ebp
  800d68:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_write, fd, base, len);
  800d6a:	ff 75 10             	pushl  0x10(%ebp)
  800d6d:	ff 75 0c             	pushl  0xc(%ebp)
  800d70:	ff 75 08             	pushl  0x8(%ebp)
  800d73:	6a 67                	push   $0x67
  800d75:	e8 03 fe ff ff       	call   800b7d <syscall>
  800d7a:	83 c4 10             	add    $0x10,%esp
}
  800d7d:	c9                   	leave  
  800d7e:	c3                   	ret    

00800d7f <sys_seek>:

int sys_seek(int fd, off_t pos, int whence)
{
  800d7f:	55                   	push   %ebp
  800d80:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_seek, fd, pos, whence);
  800d82:	ff 75 10             	pushl  0x10(%ebp)
  800d85:	ff 75 0c             	pushl  0xc(%ebp)
  800d88:	ff 75 08             	pushl  0x8(%ebp)
  800d8b:	6a 68                	push   $0x68
  800d8d:	e8 eb fd ff ff       	call   800b7d <syscall>
  800d92:	83 c4 10             	add    $0x10,%esp
}
  800d95:	c9                   	leave  
  800d96:	c3                   	ret    

00800d97 <sys_fstat>:

int sys_fstat(int fd, struct stat *stat)
{
  800d97:	55                   	push   %ebp
  800d98:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_fstat, fd, stat);
  800d9a:	ff 75 0c             	pushl  0xc(%ebp)
  800d9d:	ff 75 08             	pushl  0x8(%ebp)
  800da0:	6a 6e                	push   $0x6e
  800da2:	e8 d6 fd ff ff       	call   800b7d <syscall>
  800da7:	83 c4 0c             	add    $0xc,%esp
}
  800daa:	c9                   	leave  
  800dab:	c3                   	ret    

00800dac <sys_fsync>:

int sys_fsync(int fd)
{
  800dac:	55                   	push   %ebp
  800dad:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_fsync, fd);
  800daf:	ff 75 08             	pushl  0x8(%ebp)
  800db2:	6a 6f                	push   $0x6f
  800db4:	e8 c4 fd ff ff       	call   800b7d <syscall>
  800db9:	83 c4 08             	add    $0x8,%esp
}
  800dbc:	c9                   	leave  
  800dbd:	c3                   	ret    

00800dbe <sys_getcwd>:

int sys_getcwd(char *buffer, size_t len)
{
  800dbe:	55                   	push   %ebp
  800dbf:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_getcwd, buffer, len);
  800dc1:	ff 75 0c             	pushl  0xc(%ebp)
  800dc4:	ff 75 08             	pushl  0x8(%ebp)
  800dc7:	6a 79                	push   $0x79
  800dc9:	e8 af fd ff ff       	call   800b7d <syscall>
  800dce:	83 c4 0c             	add    $0xc,%esp
}
  800dd1:	c9                   	leave  
  800dd2:	c3                   	ret    

00800dd3 <sys_getdirentry>:

int sys_getdirentry(int fd, struct dirent *dirent)
{
  800dd3:	55                   	push   %ebp
  800dd4:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_getdirentry, fd, dirent);
  800dd6:	ff 75 0c             	pushl  0xc(%ebp)
  800dd9:	ff 75 08             	pushl  0x8(%ebp)
  800ddc:	68 80 00 00 00       	push   $0x80
  800de1:	e8 97 fd ff ff       	call   800b7d <syscall>
  800de6:	83 c4 0c             	add    $0xc,%esp
}
  800de9:	c9                   	leave  
  800dea:	c3                   	ret    

00800deb <sys_dup>:

int sys_dup(int fd1, int fd2)
{
  800deb:	55                   	push   %ebp
  800dec:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_dup, fd1, fd2);
  800dee:	ff 75 0c             	pushl  0xc(%ebp)
  800df1:	ff 75 08             	pushl  0x8(%ebp)
  800df4:	68 82 00 00 00       	push   $0x82
  800df9:	e8 7f fd ff ff       	call   800b7d <syscall>
  800dfe:	83 c4 0c             	add    $0xc,%esp
}
  800e01:	c9                   	leave  
  800e02:	c3                   	ret    

00800e03 <sys_mkdir>:

int sys_mkdir(const char *path)
{
  800e03:	55                   	push   %ebp
  800e04:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_mkdir, path);
  800e06:	ff 75 08             	pushl  0x8(%ebp)
  800e09:	6a 7a                	push   $0x7a
  800e0b:	e8 6d fd ff ff       	call   800b7d <syscall>
  800e10:	83 c4 08             	add    $0x8,%esp
}
  800e13:	c9                   	leave  
  800e14:	c3                   	ret    

00800e15 <sys_rm>:

int sys_rm(const char *path)
{
  800e15:	55                   	push   %ebp
  800e16:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_mkdir, path);
  800e18:	ff 75 08             	pushl  0x8(%ebp)
  800e1b:	6a 7a                	push   $0x7a
  800e1d:	e8 5b fd ff ff       	call   800b7d <syscall>
  800e22:	83 c4 08             	add    $0x8,%esp
}
  800e25:	c9                   	leave  
  800e26:	c3                   	ret    

00800e27 <sys_chdir>:

int sys_chdir(const char *path)
{
  800e27:	55                   	push   %ebp
  800e28:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_chdir, path);
  800e2a:	ff 75 08             	pushl  0x8(%ebp)
  800e2d:	6a 78                	push   $0x78
  800e2f:	e8 49 fd ff ff       	call   800b7d <syscall>
  800e34:	83 c4 08             	add    $0x8,%esp
}
  800e37:	c9                   	leave  
  800e38:	c3                   	ret    

00800e39 <sys_rename>:

int sys_rename(const char *path1, const char *path2)
{
  800e39:	55                   	push   %ebp
  800e3a:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_rename, path1, path2);
  800e3c:	ff 75 0c             	pushl  0xc(%ebp)
  800e3f:	ff 75 08             	pushl  0x8(%ebp)
  800e42:	6a 7c                	push   $0x7c
  800e44:	e8 34 fd ff ff       	call   800b7d <syscall>
  800e49:	83 c4 0c             	add    $0xc,%esp
}
  800e4c:	c9                   	leave  
  800e4d:	c3                   	ret    

00800e4e <sys_pipe>:

int sys_pipe(int *fd_store)
{
  800e4e:	55                   	push   %ebp
  800e4f:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_pipe, fd_store);
  800e51:	ff 75 08             	pushl  0x8(%ebp)
  800e54:	68 8c 00 00 00       	push   $0x8c
  800e59:	e8 1f fd ff ff       	call   800b7d <syscall>
  800e5e:	83 c4 08             	add    $0x8,%esp
}
  800e61:	c9                   	leave  
  800e62:	c3                   	ret    

00800e63 <sys_mkfifo>:

int sys_mkfifo(const char *name, uint32_t open_flags)
{
  800e63:	55                   	push   %ebp
  800e64:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_mkfifo, name, open_flags);
  800e66:	ff 75 0c             	pushl  0xc(%ebp)
  800e69:	ff 75 08             	pushl  0x8(%ebp)
  800e6c:	68 8d 00 00 00       	push   $0x8d
  800e71:	e8 07 fd ff ff       	call   800b7d <syscall>
  800e76:	83 c4 0c             	add    $0xc,%esp
}
  800e79:	c9                   	leave  
  800e7a:	c3                   	ret    

00800e7b <sys_link>:

int sys_link(const char *path1, const char *path2)
{
  800e7b:	55                   	push   %ebp
  800e7c:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_link, path1, path2);
  800e7e:	ff 75 0c             	pushl  0xc(%ebp)
  800e81:	ff 75 08             	pushl  0x8(%ebp)
  800e84:	6a 7b                	push   $0x7b
  800e86:	e8 f2 fc ff ff       	call   800b7d <syscall>
  800e8b:	83 c4 0c             	add    $0xc,%esp
}
  800e8e:	c9                   	leave  
  800e8f:	c3                   	ret    

00800e90 <sys_unlink>:

int sys_unlink(const char *path)
{
  800e90:	55                   	push   %ebp
  800e91:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_unlink, path);
  800e93:	ff 75 08             	pushl  0x8(%ebp)
  800e96:	6a 7f                	push   $0x7f
  800e98:	e8 e0 fc ff ff       	call   800b7d <syscall>
  800e9d:	83 c4 08             	add    $0x8,%esp
}
  800ea0:	c9                   	leave  
  800ea1:	c3                   	ret    

00800ea2 <sys_mount>:

int sys_mount(const char *source, const char *target, const char *filesystemtype, const void *data)
{
  800ea2:	55                   	push   %ebp
  800ea3:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_mount, source, target, filesystemtype, data);
  800ea5:	ff 75 14             	pushl  0x14(%ebp)
  800ea8:	ff 75 10             	pushl  0x10(%ebp)
  800eab:	ff 75 0c             	pushl  0xc(%ebp)
  800eae:	ff 75 08             	pushl  0x8(%ebp)
  800eb1:	68 83 00 00 00       	push   $0x83
  800eb6:	e8 c2 fc ff ff       	call   800b7d <syscall>
  800ebb:	83 c4 14             	add    $0x14,%esp
}
  800ebe:	c9                   	leave  
  800ebf:	c3                   	ret    

00800ec0 <sys_umount>:

int sys_umount(const char *target)
{
  800ec0:	55                   	push   %ebp
  800ec1:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_umount, target);
  800ec3:	ff 75 08             	pushl  0x8(%ebp)
  800ec6:	68 84 00 00 00       	push   $0x84
  800ecb:	e8 ad fc ff ff       	call   800b7d <syscall>
  800ed0:	83 c4 08             	add    $0x8,%esp
}
  800ed3:	c9                   	leave  
  800ed4:	c3                   	ret    

00800ed5 <sys_sigaction>:

int sys_sigaction(int sign, struct sigaction *act, struct sigaction *old)
{
  800ed5:	55                   	push   %ebp
  800ed6:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sigaction, sign, act, old);
  800ed8:	ff 75 10             	pushl  0x10(%ebp)
  800edb:	ff 75 0c             	pushl  0xc(%ebp)
  800ede:	ff 75 08             	pushl  0x8(%ebp)
  800ee1:	68 90 00 00 00       	push   $0x90
  800ee6:	e8 92 fc ff ff       	call   800b7d <syscall>
  800eeb:	83 c4 10             	add    $0x10,%esp
}
  800eee:	c9                   	leave  
  800eef:	c3                   	ret    

00800ef0 <sys_sigtkill>:

int sys_sigtkill(int pid, int sign)
{
  800ef0:	55                   	push   %ebp
  800ef1:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_tkill, pid, sign);
  800ef3:	ff 75 0c             	pushl  0xc(%ebp)
  800ef6:	ff 75 08             	pushl  0x8(%ebp)
  800ef9:	68 8f 00 00 00       	push   $0x8f
  800efe:	e8 7a fc ff ff       	call   800b7d <syscall>
  800f03:	83 c4 0c             	add    $0xc,%esp
}
  800f06:	c9                   	leave  
  800f07:	c3                   	ret    

00800f08 <sys_sigprocmask>:

int sys_sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
  800f08:	55                   	push   %ebp
  800f09:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sigprocmask, how, set, old);
  800f0b:	ff 75 10             	pushl  0x10(%ebp)
  800f0e:	ff 75 0c             	pushl  0xc(%ebp)
  800f11:	ff 75 08             	pushl  0x8(%ebp)
  800f14:	68 91 00 00 00       	push   $0x91
  800f19:	e8 5f fc ff ff       	call   800b7d <syscall>
  800f1e:	83 c4 10             	add    $0x10,%esp
}
  800f21:	c9                   	leave  
  800f22:	c3                   	ret    

00800f23 <sys_sigsuspend>:

int sys_sigsuspend(uint32_t mask)
{
  800f23:	55                   	push   %ebp
  800f24:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sigsuspend, mask);
  800f26:	ff 75 08             	pushl  0x8(%ebp)
  800f29:	68 92 00 00 00       	push   $0x92
  800f2e:	e8 4a fc ff ff       	call   800b7d <syscall>
  800f33:	83 c4 08             	add    $0x8,%esp
}
  800f36:	c9                   	leave  
  800f37:	c3                   	ret    

00800f38 <sys_sigreturn>:

int sys_sigreturn(uintptr_t sp)
{
  800f38:	55                   	push   %ebp
  800f39:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sigreturn, sp);
  800f3b:	ff 75 08             	pushl  0x8(%ebp)
  800f3e:	68 93 00 00 00       	push   $0x93
  800f43:	e8 35 fc ff ff       	call   800b7d <syscall>
  800f48:	83 c4 08             	add    $0x8,%esp
}
  800f4b:	c9                   	leave  
  800f4c:	c3                   	ret    

00800f4d <sys_receive_packet>:

int sys_receive_packet(uint8_t *buf, size_t len, size_t* len_store)
{
  800f4d:	55                   	push   %ebp
  800f4e:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_receive_packet, (int)buf, len, (int)len_store, 0, 0 );
  800f50:	8b 55 10             	mov    0x10(%ebp),%edx
  800f53:	8b 45 08             	mov    0x8(%ebp),%eax
  800f56:	6a 00                	push   $0x0
  800f58:	6a 00                	push   $0x0
  800f5a:	52                   	push   %edx
  800f5b:	ff 75 0c             	pushl  0xc(%ebp)
  800f5e:	50                   	push   %eax
  800f5f:	68 97 00 00 00       	push   $0x97
  800f64:	e8 14 fc ff ff       	call   800b7d <syscall>
  800f69:	83 c4 18             	add    $0x18,%esp
}
  800f6c:	c9                   	leave  
  800f6d:	c3                   	ret    

00800f6e <sys_transmit_packet>:

int sys_transmit_packet(uint8_t *buf, size_t len,size_t* len_store)
{
  800f6e:	55                   	push   %ebp
  800f6f:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_transmit_packet, (int)buf, len, (int)len_store, 0, 0);
  800f71:	8b 55 10             	mov    0x10(%ebp),%edx
  800f74:	8b 45 08             	mov    0x8(%ebp),%eax
  800f77:	6a 00                	push   $0x0
  800f79:	6a 00                	push   $0x0
  800f7b:	52                   	push   %edx
  800f7c:	ff 75 0c             	pushl  0xc(%ebp)
  800f7f:	50                   	push   %eax
  800f80:	68 96 00 00 00       	push   $0x96
  800f85:	e8 f3 fb ff ff       	call   800b7d <syscall>
  800f8a:	83 c4 18             	add    $0x18,%esp
}
  800f8d:	c9                   	leave  
  800f8e:	c3                   	ret    

00800f8f <sys_send_event>:

int sys_send_event(int pid, int event_type, int event)
{
  800f8f:	55                   	push   %ebp
  800f90:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_event_send, pid, event_type, event, 0, 0);
  800f92:	6a 00                	push   $0x0
  800f94:	6a 00                	push   $0x0
  800f96:	ff 75 10             	pushl  0x10(%ebp)
  800f99:	ff 75 0c             	pushl  0xc(%ebp)
  800f9c:	ff 75 08             	pushl  0x8(%ebp)
  800f9f:	6a 30                	push   $0x30
  800fa1:	e8 d7 fb ff ff       	call   800b7d <syscall>
  800fa6:	83 c4 18             	add    $0x18,%esp
}
  800fa9:	c9                   	leave  
  800faa:	c3                   	ret    

00800fab <sys_recv_event>:

int sys_recv_event(int *pid_store, int event_type, int *event_store, unsigned int timeout)
{
  800fab:	55                   	push   %ebp
  800fac:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_event_recv, (int)pid_store, (int)event_type, (int)event_store, timeout, 0, 0);
  800fae:	8b 55 10             	mov    0x10(%ebp),%edx
  800fb1:	8b 45 08             	mov    0x8(%ebp),%eax
  800fb4:	6a 00                	push   $0x0
  800fb6:	6a 00                	push   $0x0
  800fb8:	ff 75 14             	pushl  0x14(%ebp)
  800fbb:	52                   	push   %edx
  800fbc:	ff 75 0c             	pushl  0xc(%ebp)
  800fbf:	50                   	push   %eax
  800fc0:	6a 31                	push   $0x31
  800fc2:	e8 b6 fb ff ff       	call   800b7d <syscall>
  800fc7:	83 c4 1c             	add    $0x1c,%esp
}
  800fca:	c9                   	leave  
  800fcb:	c3                   	ret    

00800fcc <sys_mbox_init>:

int sys_mbox_init(unsigned int max_slots)
{
  800fcc:	55                   	push   %ebp
  800fcd:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_mbox_init, max_slots, 0, 0, 0, 0);
  800fcf:	6a 00                	push   $0x0
  800fd1:	6a 00                	push   $0x0
  800fd3:	6a 00                	push   $0x0
  800fd5:	6a 00                	push   $0x0
  800fd7:	ff 75 08             	pushl  0x8(%ebp)
  800fda:	6a 32                	push   $0x32
  800fdc:	e8 9c fb ff ff       	call   800b7d <syscall>
  800fe1:	83 c4 18             	add    $0x18,%esp
}
  800fe4:	c9                   	leave  
  800fe5:	c3                   	ret    

00800fe6 <sys_mbox_send>:

int sys_mbox_send(int id, struct mboxbuf *buf, unsigned int timeout)
{
  800fe6:	55                   	push   %ebp
  800fe7:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_mbox_send, id, (int)buf, timeout, 0, 0);
  800fe9:	8b 45 0c             	mov    0xc(%ebp),%eax
  800fec:	6a 00                	push   $0x0
  800fee:	6a 00                	push   $0x0
  800ff0:	ff 75 10             	pushl  0x10(%ebp)
  800ff3:	50                   	push   %eax
  800ff4:	ff 75 08             	pushl  0x8(%ebp)
  800ff7:	6a 33                	push   $0x33
  800ff9:	e8 7f fb ff ff       	call   800b7d <syscall>
  800ffe:	83 c4 18             	add    $0x18,%esp
}
  801001:	c9                   	leave  
  801002:	c3                   	ret    

00801003 <sys_mbox_recv>:

int sys_mbox_recv(int id, struct mboxbuf *buf, unsigned int timeout)
{
  801003:	55                   	push   %ebp
  801004:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_mbox_recv, id, (int)buf, timeout, 0, 0);
  801006:	8b 45 0c             	mov    0xc(%ebp),%eax
  801009:	6a 00                	push   $0x0
  80100b:	6a 00                	push   $0x0
  80100d:	ff 75 10             	pushl  0x10(%ebp)
  801010:	50                   	push   %eax
  801011:	ff 75 08             	pushl  0x8(%ebp)
  801014:	6a 34                	push   $0x34
  801016:	e8 62 fb ff ff       	call   800b7d <syscall>
  80101b:	83 c4 18             	add    $0x18,%esp
}
  80101e:	c9                   	leave  
  80101f:	c3                   	ret    

00801020 <sys_mbox_free>:

int sys_mbox_free(int id)
{
  801020:	55                   	push   %ebp
  801021:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_mbox_free, id, 0, 0, 0, 0);
  801023:	6a 00                	push   $0x0
  801025:	6a 00                	push   $0x0
  801027:	6a 00                	push   $0x0
  801029:	6a 00                	push   $0x0
  80102b:	ff 75 08             	pushl  0x8(%ebp)
  80102e:	6a 35                	push   $0x35
  801030:	e8 48 fb ff ff       	call   800b7d <syscall>
  801035:	83 c4 18             	add    $0x18,%esp
}
  801038:	c9                   	leave  
  801039:	c3                   	ret    

0080103a <sys_mbox_info>:

int sys_mbox_info(int id, struct mboxinfo *info)
{
  80103a:	55                   	push   %ebp
  80103b:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_mbox_info, id, (int)info, 0, 0, 0);
  80103d:	8b 45 0c             	mov    0xc(%ebp),%eax
  801040:	6a 00                	push   $0x0
  801042:	6a 00                	push   $0x0
  801044:	6a 00                	push   $0x0
  801046:	50                   	push   %eax
  801047:	ff 75 08             	pushl  0x8(%ebp)
  80104a:	6a 36                	push   $0x36
  80104c:	e8 2c fb ff ff       	call   800b7d <syscall>
  801051:	83 c4 18             	add    $0x18,%esp
}
  801054:	c9                   	leave  
  801055:	c3                   	ret    

00801056 <sys_ping>:

int sys_ping(char *target, int len)
{
  801056:	55                   	push   %ebp
  801057:	89 e5                	mov    %esp,%ebp
   return syscall(SYS_ping, (int)target, len, 0);
  801059:	8b 45 08             	mov    0x8(%ebp),%eax
  80105c:	6a 00                	push   $0x0
  80105e:	ff 75 0c             	pushl  0xc(%ebp)
  801061:	50                   	push   %eax
  801062:	68 98 00 00 00       	push   $0x98
  801067:	e8 11 fb ff ff       	call   800b7d <syscall>
  80106c:	83 c4 10             	add    $0x10,%esp
}
  80106f:	c9                   	leave  
  801070:	c3                   	ret    

00801071 <sys_process_dump>:

int sys_process_dump()
{
  801071:	55                   	push   %ebp
  801072:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_process_dump, 0, 0, 0);
  801074:	6a 00                	push   $0x0
  801076:	6a 00                	push   $0x0
  801078:	6a 00                	push   $0x0
  80107a:	68 99 00 00 00       	push   $0x99
  80107f:	e8 f9 fa ff ff       	call   800b7d <syscall>
  801084:	83 c4 10             	add    $0x10,%esp
}
  801087:	c9                   	leave  
  801088:	c3                   	ret    

00801089 <sys_rtdump>:

int sys_rtdump()
{
  801089:	55                   	push   %ebp
  80108a:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_rtdump, 0, 0, 0);
  80108c:	6a 00                	push   $0x0
  80108e:	6a 00                	push   $0x0
  801090:	6a 00                	push   $0x0
  801092:	68 9a 00 00 00       	push   $0x9a
  801097:	e8 e1 fa ff ff       	call   800b7d <syscall>
  80109c:	83 c4 10             	add    $0x10,%esp
}
  80109f:	c9                   	leave  
  8010a0:	c3                   	ret    

008010a1 <sys_arpprint>:

int sys_arpprint()
{
  8010a1:	55                   	push   %ebp
  8010a2:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_arpprint, 0, 0, 0);
  8010a4:	6a 00                	push   $0x0
  8010a6:	6a 00                	push   $0x0
  8010a8:	6a 00                	push   $0x0
  8010aa:	68 9b 00 00 00       	push   $0x9b
  8010af:	e8 c9 fa ff ff       	call   800b7d <syscall>
  8010b4:	83 c4 10             	add    $0x10,%esp
}
  8010b7:	c9                   	leave  
  8010b8:	c3                   	ret    

008010b9 <sys_netstatus>:

int sys_netstatus()
{
  8010b9:	55                   	push   %ebp
  8010ba:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_netstatus, 0, 0, 0);
  8010bc:	6a 00                	push   $0x0
  8010be:	6a 00                	push   $0x0
  8010c0:	6a 00                	push   $0x0
  8010c2:	68 9c 00 00 00       	push   $0x9c
  8010c7:	e8 b1 fa ff ff       	call   800b7d <syscall>
  8010cc:	83 c4 10             	add    $0x10,%esp
}
  8010cf:	c9                   	leave  
  8010d0:	c3                   	ret    

008010d1 <sys_sock_socket>:

int sys_sock_socket(uint32_t type, const char* ipaddr, uint32_t iplen)
{
  8010d1:	55                   	push   %ebp
  8010d2:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sock_socket, type, (int)ipaddr , iplen, 0, 0);
  8010d4:	8b 45 0c             	mov    0xc(%ebp),%eax
  8010d7:	6a 00                	push   $0x0
  8010d9:	6a 00                	push   $0x0
  8010db:	ff 75 10             	pushl  0x10(%ebp)
  8010de:	50                   	push   %eax
  8010df:	ff 75 08             	pushl  0x8(%ebp)
  8010e2:	68 c8 00 00 00       	push   $0xc8
  8010e7:	e8 91 fa ff ff       	call   800b7d <syscall>
  8010ec:	83 c4 18             	add    $0x18,%esp
}
  8010ef:	c9                   	leave  
  8010f0:	c3                   	ret    

008010f1 <sys_sock_listen>:

int sys_sock_listen(uint32_t tcpfd, uint32_t qsize)
{
  8010f1:	55                   	push   %ebp
  8010f2:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sock_listen, tcpfd, qsize, 0);
  8010f4:	6a 00                	push   $0x0
  8010f6:	ff 75 0c             	pushl  0xc(%ebp)
  8010f9:	ff 75 08             	pushl  0x8(%ebp)
  8010fc:	68 c9 00 00 00       	push   $0xc9
  801101:	e8 77 fa ff ff       	call   800b7d <syscall>
  801106:	83 c4 10             	add    $0x10,%esp
}
  801109:	c9                   	leave  
  80110a:	c3                   	ret    

0080110b <sys_sock_accept>:

int sys_sock_accept(uint32_t listenfd, uint32_t timeout)
{
  80110b:	55                   	push   %ebp
  80110c:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sock_accept, listenfd, timeout, 0);
  80110e:	6a 00                	push   $0x0
  801110:	ff 75 0c             	pushl  0xc(%ebp)
  801113:	ff 75 08             	pushl  0x8(%ebp)
  801116:	68 ca 00 00 00       	push   $0xca
  80111b:	e8 5d fa ff ff       	call   800b7d <syscall>
  801120:	83 c4 10             	add    $0x10,%esp
}
  801123:	c9                   	leave  
  801124:	c3                   	ret    

00801125 <sys_sock_connect>:

int sys_sock_connect(uint32_t sockfd, const char* ipaddr, uint32_t iplen)
{
  801125:	55                   	push   %ebp
  801126:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sock_connect, sockfd, (int)ipaddr, iplen, 0, 0);
  801128:	8b 45 0c             	mov    0xc(%ebp),%eax
  80112b:	6a 00                	push   $0x0
  80112d:	6a 00                	push   $0x0
  80112f:	ff 75 10             	pushl  0x10(%ebp)
  801132:	50                   	push   %eax
  801133:	ff 75 08             	pushl  0x8(%ebp)
  801136:	68 cb 00 00 00       	push   $0xcb
  80113b:	e8 3d fa ff ff       	call   800b7d <syscall>
  801140:	83 c4 18             	add    $0x18,%esp
}
  801143:	c9                   	leave  
  801144:	c3                   	ret    

00801145 <sys_sock_bind>:

int sys_sock_bind(uint32_t sockfd, uint32_t lport, uint32_t rport)
{
  801145:	55                   	push   %ebp
  801146:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sock_bind, sockfd, lport, rport, 0, 0);
  801148:	6a 00                	push   $0x0
  80114a:	6a 00                	push   $0x0
  80114c:	ff 75 10             	pushl  0x10(%ebp)
  80114f:	ff 75 0c             	pushl  0xc(%ebp)
  801152:	ff 75 08             	pushl  0x8(%ebp)
  801155:	68 cc 00 00 00       	push   $0xcc
  80115a:	e8 1e fa ff ff       	call   800b7d <syscall>
  80115f:	83 c4 18             	add    $0x18,%esp
}
  801162:	c9                   	leave  
  801163:	c3                   	ret    

00801164 <sys_sock_send>:

int sys_sock_send(uint32_t sockfd, char* buf, uint32_t len, uint32_t timeout)
{
  801164:	55                   	push   %ebp
  801165:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sock_send, sockfd, (int)buf, len, timeout, 0);
  801167:	8b 45 0c             	mov    0xc(%ebp),%eax
  80116a:	6a 00                	push   $0x0
  80116c:	ff 75 14             	pushl  0x14(%ebp)
  80116f:	ff 75 10             	pushl  0x10(%ebp)
  801172:	50                   	push   %eax
  801173:	ff 75 08             	pushl  0x8(%ebp)
  801176:	68 cd 00 00 00       	push   $0xcd
  80117b:	e8 fd f9 ff ff       	call   800b7d <syscall>
  801180:	83 c4 18             	add    $0x18,%esp
}
  801183:	c9                   	leave  
  801184:	c3                   	ret    

00801185 <sys_sock_recv>:

int sys_sock_recv(uint32_t sockfd, char* buf, uint32_t len, uint32_t timeout)
{
  801185:	55                   	push   %ebp
  801186:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sock_recv, sockfd, (int)buf, len, timeout, 0);
  801188:	8b 45 0c             	mov    0xc(%ebp),%eax
  80118b:	6a 00                	push   $0x0
  80118d:	ff 75 14             	pushl  0x14(%ebp)
  801190:	ff 75 10             	pushl  0x10(%ebp)
  801193:	50                   	push   %eax
  801194:	ff 75 08             	pushl  0x8(%ebp)
  801197:	68 ce 00 00 00       	push   $0xce
  80119c:	e8 dc f9 ff ff       	call   800b7d <syscall>
  8011a1:	83 c4 18             	add    $0x18,%esp
}
  8011a4:	c9                   	leave  
  8011a5:	c3                   	ret    

008011a6 <sys_sock_close>:

int sys_sock_close(uint32_t sockfd)
{
  8011a6:	55                   	push   %ebp
  8011a7:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sock_close, sockfd, 0, 0);
  8011a9:	6a 00                	push   $0x0
  8011ab:	6a 00                	push   $0x0
  8011ad:	ff 75 08             	pushl  0x8(%ebp)
  8011b0:	68 cf 00 00 00       	push   $0xcf
  8011b5:	e8 c3 f9 ff ff       	call   800b7d <syscall>
  8011ba:	83 c4 10             	add    $0x10,%esp
}
  8011bd:	c9                   	leave  
  8011be:	c3                   	ret    

008011bf <sys_sock_shutdown>:

int sys_sock_shutdown(uint32_t sockfd, uint32_t type)
{
  8011bf:	55                   	push   %ebp
  8011c0:	89 e5                	mov    %esp,%ebp
    return syscall(SYS_sock_shutdown, sockfd, type, 0);
  8011c2:	6a 00                	push   $0x0
  8011c4:	ff 75 0c             	pushl  0xc(%ebp)
  8011c7:	ff 75 08             	pushl  0x8(%ebp)
  8011ca:	68 d0 00 00 00       	push   $0xd0
  8011cf:	e8 a9 f9 ff ff       	call   800b7d <syscall>
  8011d4:	83 c4 10             	add    $0x10,%esp
}
  8011d7:	c9                   	leave  
  8011d8:	c3                   	ret    

008011d9 <thread>:
#include "thread.h"
#include "unistd.h"
#include "error.h"

int thread(int (*fn)(void *), void *arg, thread_t *tidp)
{
  8011d9:	55                   	push   %ebp
  8011da:	89 e5                	mov    %esp,%ebp
  8011dc:	83 ec 18             	sub    $0x18,%esp
	if (fn == NULL || tidp == NULL)
  8011df:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
  8011e3:	74 06                	je     8011eb <thread+0x12>
  8011e5:	83 7d 10 00          	cmpl   $0x0,0x10(%ebp)
  8011e9:	75 0a                	jne    8011f5 <thread+0x1c>
    {
		return -E_INVAL;
  8011eb:	b8 fd ff ff ff       	mov    $0xfffffffd,%eax
  8011f0:	e9 ab 00 00 00       	jmp    8012a0 <thread+0xc7>
	}
    
    // 从进程栈空间地址往下（mmap 区域），寻找可用的虚拟地址空间，作为线程的栈空间
    // 这里只是分配了虚拟地址空间，线程在开始运行使用堆栈时，会发生缺页中断，从而申请内存
	int ret;
	uintptr_t stack = 0;
  8011f5:	c7 45 f0 00 00 00 00 	movl   $0x0,-0x10(%ebp)
	if ((ret = mmap(&stack, THREAD_STACKSIZE, MMAP_WRITE | MMAP_STACK)) != 0)
  8011fc:	83 ec 04             	sub    $0x4,%esp
  8011ff:	68 00 03 00 00       	push   $0x300
  801204:	68 00 a0 00 00       	push   $0xa000
  801209:	8d 45 f0             	lea    -0x10(%ebp),%eax
  80120c:	50                   	push   %eax
  80120d:	e8 ca 02 00 00       	call   8014dc <mmap>
  801212:	83 c4 10             	add    $0x10,%esp
  801215:	89 45 f4             	mov    %eax,-0xc(%ebp)
  801218:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
  80121c:	74 05                	je     801223 <thread+0x4a>
    {
		return ret;
  80121e:	8b 45 f4             	mov    -0xc(%ebp),%eax
  801221:	eb 7d                	jmp    8012a0 <thread+0xc7>
	}
	assert(stack != 0);
  801223:	8b 45 f0             	mov    -0x10(%ebp),%eax
  801226:	85 c0                	test   %eax,%eax
  801228:	75 16                	jne    801240 <thread+0x67>
  80122a:	68 05 29 80 00       	push   $0x802905
  80122f:	68 10 29 80 00       	push   $0x802910
  801234:	6a 16                	push   $0x16
  801236:	68 25 29 80 00       	push   $0x802925
  80123b:	e8 66 f6 ff ff       	call   8008a6 <__panic>
    
    print_pgdir();
  801240:	e8 6d 02 00 00       	call   8014b2 <print_pgdir>
    print_vm();
  801245:	e8 76 02 00 00       	call   8014c0 <print_vm>
    
	if ((ret = clone(CLONE_VM | CLONE_THREAD | CLONE_FS, stack + THREAD_STACKSIZE, fn, arg)) < 0)
  80124a:	8b 45 f0             	mov    -0x10(%ebp),%eax
  80124d:	05 00 a0 00 00       	add    $0xa000,%eax
  801252:	ff 75 0c             	pushl  0xc(%ebp)
  801255:	ff 75 08             	pushl  0x8(%ebp)
  801258:	50                   	push   %eax
  801259:	68 00 03 01 00       	push   $0x10300
  80125e:	e8 c2 01 00 00       	call   801425 <clone>
  801263:	83 c4 10             	add    $0x10,%esp
  801266:	89 45 f4             	mov    %eax,-0xc(%ebp)
  801269:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
  80126d:	79 19                	jns    801288 <thread+0xaf>
    {
		munmap(stack, THREAD_STACKSIZE);
  80126f:	8b 45 f0             	mov    -0x10(%ebp),%eax
  801272:	83 ec 08             	sub    $0x8,%esp
  801275:	68 00 a0 00 00       	push   $0xa000
  80127a:	50                   	push   %eax
  80127b:	e8 78 02 00 00       	call   8014f8 <munmap>
  801280:	83 c4 10             	add    $0x10,%esp
		return ret;
  801283:	8b 45 f4             	mov    -0xc(%ebp),%eax
  801286:	eb 18                	jmp    8012a0 <thread+0xc7>
	}

	tidp->pid = ret;
  801288:	8b 45 10             	mov    0x10(%ebp),%eax
  80128b:	8b 55 f4             	mov    -0xc(%ebp),%edx
  80128e:	89 10                	mov    %edx,(%eax)
	tidp->stack = (void *)stack;
  801290:	8b 45 f0             	mov    -0x10(%ebp),%eax
  801293:	89 c2                	mov    %eax,%edx
  801295:	8b 45 10             	mov    0x10(%ebp),%eax
  801298:	89 50 04             	mov    %edx,0x4(%eax)
	return 0;
  80129b:	b8 00 00 00 00       	mov    $0x0,%eax
}
  8012a0:	c9                   	leave  
  8012a1:	c3                   	ret    

008012a2 <thread_wait>:

int thread_wait(thread_t * tidp, int *exit_code)
{
  8012a2:	55                   	push   %ebp
  8012a3:	89 e5                	mov    %esp,%ebp
  8012a5:	83 ec 18             	sub    $0x18,%esp
	int ret = -E_INVAL;
  8012a8:	c7 45 f4 fd ff ff ff 	movl   $0xfffffffd,-0xc(%ebp)
	if (tidp != NULL)
  8012af:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
  8012b3:	74 34                	je     8012e9 <thread_wait+0x47>
    {
		if ((ret = waitpid(tidp->pid, exit_code)) == 0)
  8012b5:	8b 45 08             	mov    0x8(%ebp),%eax
  8012b8:	8b 00                	mov    (%eax),%eax
  8012ba:	83 ec 08             	sub    $0x8,%esp
  8012bd:	ff 75 0c             	pushl  0xc(%ebp)
  8012c0:	50                   	push   %eax
  8012c1:	e8 a2 01 00 00       	call   801468 <waitpid>
  8012c6:	83 c4 10             	add    $0x10,%esp
  8012c9:	89 45 f4             	mov    %eax,-0xc(%ebp)
  8012cc:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
  8012d0:	75 17                	jne    8012e9 <thread_wait+0x47>
        {
			munmap((uintptr_t) (tidp->stack), THREAD_STACKSIZE);
  8012d2:	8b 45 08             	mov    0x8(%ebp),%eax
  8012d5:	8b 40 04             	mov    0x4(%eax),%eax
  8012d8:	83 ec 08             	sub    $0x8,%esp
  8012db:	68 00 a0 00 00       	push   $0xa000
  8012e0:	50                   	push   %eax
  8012e1:	e8 12 02 00 00       	call   8014f8 <munmap>
  8012e6:	83 c4 10             	add    $0x10,%esp
		}
	}
	return ret;
  8012e9:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
  8012ec:	c9                   	leave  
  8012ed:	c3                   	ret    

008012ee <thread_kill>:

int thread_kill(thread_t * tidp)
{
  8012ee:	55                   	push   %ebp
  8012ef:	89 e5                	mov    %esp,%ebp
  8012f1:	83 ec 08             	sub    $0x8,%esp
	if (tidp != NULL)
  8012f4:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
  8012f8:	74 13                	je     80130d <thread_kill+0x1f>
    {
		return kill(tidp->pid);
  8012fa:	8b 45 08             	mov    0x8(%ebp),%eax
  8012fd:	8b 00                	mov    (%eax),%eax
  8012ff:	83 ec 0c             	sub    $0xc,%esp
  801302:	50                   	push   %eax
  801303:	e8 87 01 00 00       	call   80148f <kill>
  801308:	83 c4 10             	add    $0x10,%esp
  80130b:	eb 05                	jmp    801312 <thread_kill+0x24>
	}
	return -E_INVAL;
  80130d:	b8 fd ff ff ff       	mov    $0xfffffffd,%eax
}
  801312:	c9                   	leave  
  801313:	c3                   	ret    

00801314 <try_lock>:
{
  801314:	55                   	push   %ebp
  801315:	89 e5                	mov    %esp,%ebp
  801317:	83 ec 10             	sub    $0x10,%esp
  80131a:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%ebp)
  801321:	8b 45 08             	mov    0x8(%ebp),%eax
  801324:	89 45 f8             	mov    %eax,-0x8(%ebp)
    asm volatile ("btsl %2, %1; sbbl %0, %0" : "=r" (oldbit), "=m" (*(volatile long *)addr) : "Ir" (nr) : "memory");
  801327:	8b 55 f8             	mov    -0x8(%ebp),%edx
  80132a:	8b 45 fc             	mov    -0x4(%ebp),%eax
  80132d:	0f ab 02             	bts    %eax,(%edx)
  801330:	19 c0                	sbb    %eax,%eax
  801332:	89 45 f4             	mov    %eax,-0xc(%ebp)
    return oldbit != 0;
  801335:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
  801339:	0f 95 c0             	setne  %al
  80133c:	0f b6 c0             	movzbl %al,%eax
    return test_and_set_bit(0, l);
  80133f:	90                   	nop
}
  801340:	c9                   	leave  
  801341:	c3                   	ret    

00801342 <lock>:
{
  801342:	55                   	push   %ebp
  801343:	89 e5                	mov    %esp,%ebp
  801345:	83 ec 18             	sub    $0x18,%esp
    if (try_lock(l))
  801348:	ff 75 08             	pushl  0x8(%ebp)
  80134b:	e8 c4 ff ff ff       	call   801314 <try_lock>
  801350:	83 c4 04             	add    $0x4,%esp
  801353:	85 c0                	test   %eax,%eax
  801355:	74 3b                	je     801392 <lock+0x50>
        int step = 0;
  801357:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
            yield();
  80135e:	e8 1e 01 00 00       	call   801481 <yield>
            if (++ step == 100)
  801363:	ff 45 f4             	incl   -0xc(%ebp)
  801366:	83 7d f4 64          	cmpl   $0x64,-0xc(%ebp)
  80136a:	75 14                	jne    801380 <lock+0x3e>
                step = 0;
  80136c:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
                sleep(10);
  801373:	83 ec 0c             	sub    $0xc,%esp
  801376:	6a 0a                	push   $0xa
  801378:	e8 ab 01 00 00       	call   801528 <sleep>
  80137d:	83 c4 10             	add    $0x10,%esp
        } while (try_lock(l));
  801380:	83 ec 0c             	sub    $0xc,%esp
  801383:	ff 75 08             	pushl  0x8(%ebp)
  801386:	e8 89 ff ff ff       	call   801314 <try_lock>
  80138b:	83 c4 10             	add    $0x10,%esp
  80138e:	85 c0                	test   %eax,%eax
  801390:	75 cc                	jne    80135e <lock+0x1c>
}
  801392:	90                   	nop
  801393:	c9                   	leave  
  801394:	c3                   	ret    

00801395 <unlock>:
{
  801395:	55                   	push   %ebp
  801396:	89 e5                	mov    %esp,%ebp
  801398:	83 ec 10             	sub    $0x10,%esp
  80139b:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%ebp)
  8013a2:	8b 45 08             	mov    0x8(%ebp),%eax
  8013a5:	89 45 f8             	mov    %eax,-0x8(%ebp)
    asm volatile ("btrl %2, %1; sbbl %0, %0" : "=r" (oldbit), "=m" (*(volatile long *)addr) : "Ir" (nr) : "memory");
  8013a8:	8b 55 f8             	mov    -0x8(%ebp),%edx
  8013ab:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8013ae:	0f b3 02             	btr    %eax,(%edx)
  8013b1:	19 c0                	sbb    %eax,%eax
  8013b3:	89 45 f4             	mov    %eax,-0xc(%ebp)
    return oldbit != 0;
  8013b6:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
}
  8013ba:	90                   	nop
  8013bb:	c9                   	leave  
  8013bc:	c3                   	ret    

008013bd <lock_fork>:
#include "syscall.h"

static lock_t fork_lock = INIT_LOCK;

void lock_fork(void)
{
  8013bd:	55                   	push   %ebp
  8013be:	89 e5                	mov    %esp,%ebp
  8013c0:	83 ec 08             	sub    $0x8,%esp
    lock(&fork_lock);
  8013c3:	83 ec 0c             	sub    $0xc,%esp
  8013c6:	68 0c 31 80 00       	push   $0x80310c
  8013cb:	e8 72 ff ff ff       	call   801342 <lock>
  8013d0:	83 c4 10             	add    $0x10,%esp
}
  8013d3:	90                   	nop
  8013d4:	c9                   	leave  
  8013d5:	c3                   	ret    

008013d6 <unlock_fork>:

void unlock_fork(void)
{
  8013d6:	55                   	push   %ebp
  8013d7:	89 e5                	mov    %esp,%ebp
    unlock(&fork_lock);
  8013d9:	68 0c 31 80 00       	push   $0x80310c
  8013de:	e8 b2 ff ff ff       	call   801395 <unlock>
  8013e3:	83 c4 04             	add    $0x4,%esp
}
  8013e6:	90                   	nop
  8013e7:	c9                   	leave  
  8013e8:	c3                   	ret    

008013e9 <exit>:

void exit(int error_code)
{
  8013e9:	55                   	push   %ebp
  8013ea:	89 e5                	mov    %esp,%ebp
  8013ec:	83 ec 08             	sub    $0x8,%esp
    sys_exit(error_code);
  8013ef:	83 ec 0c             	sub    $0xc,%esp
  8013f2:	ff 75 08             	pushl  0x8(%ebp)
  8013f5:	e8 d8 f7 ff ff       	call   800bd2 <sys_exit>
  8013fa:	83 c4 10             	add    $0x10,%esp
    cprintf("BUG: exit failed.\n");
  8013fd:	83 ec 0c             	sub    $0xc,%esp
  801400:	68 38 29 80 00       	push   $0x802938
  801405:	e8 78 f6 ff ff       	call   800a82 <cprintf>
  80140a:	83 c4 10             	add    $0x10,%esp
    while (1);
  80140d:	eb fe                	jmp    80140d <exit+0x24>

0080140f <fork>:
}

// 这里修改下 fork，加个参数，给生成的子进程设置名字，方便 debug
int fork(char *name)
{
  80140f:	55                   	push   %ebp
  801410:	89 e5                	mov    %esp,%ebp
  801412:	83 ec 08             	sub    $0x8,%esp
    return sys_fork(name);
  801415:	83 ec 0c             	sub    $0xc,%esp
  801418:	ff 75 08             	pushl  0x8(%ebp)
  80141b:	e8 c4 f7 ff ff       	call   800be4 <sys_fork>
  801420:	83 c4 10             	add    $0x10,%esp
}
  801423:	c9                   	leave  
  801424:	c3                   	ret    

00801425 <clone>:

int __clone(uint32_t clone_flags, uintptr_t stack, int (*fn)(void *), void *arg);

int clone(uint32_t clone_flags, uintptr_t stack, int (*fn)(void *), void *arg)
{
  801425:	55                   	push   %ebp
  801426:	89 e5                	mov    %esp,%ebp
  801428:	83 ec 18             	sub    $0x18,%esp
    int ret;
    lock_fork();
  80142b:	e8 8d ff ff ff       	call   8013bd <lock_fork>
    ret = __clone(clone_flags, stack, fn, arg);
  801430:	ff 75 14             	pushl  0x14(%ebp)
  801433:	ff 75 10             	pushl  0x10(%ebp)
  801436:	ff 75 0c             	pushl  0xc(%ebp)
  801439:	ff 75 08             	pushl  0x8(%ebp)
  80143c:	e8 df eb ff ff       	call   800020 <__clone>
  801441:	83 c4 10             	add    $0x10,%esp
  801444:	89 45 f4             	mov    %eax,-0xc(%ebp)
    unlock_fork();
  801447:	e8 8a ff ff ff       	call   8013d6 <unlock_fork>
    return ret;
  80144c:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
  80144f:	c9                   	leave  
  801450:	c3                   	ret    

00801451 <wait>:

int wait(void)
{
  801451:	55                   	push   %ebp
  801452:	89 e5                	mov    %esp,%ebp
  801454:	83 ec 08             	sub    $0x8,%esp
    return sys_wait(0, NULL);
  801457:	83 ec 08             	sub    $0x8,%esp
  80145a:	6a 00                	push   $0x0
  80145c:	6a 00                	push   $0x0
  80145e:	e8 93 f7 ff ff       	call   800bf6 <sys_wait>
  801463:	83 c4 10             	add    $0x10,%esp
}
  801466:	c9                   	leave  
  801467:	c3                   	ret    

00801468 <waitpid>:

int waitpid(int pid, int *store)
{
  801468:	55                   	push   %ebp
  801469:	89 e5                	mov    %esp,%ebp
  80146b:	83 ec 08             	sub    $0x8,%esp
    return sys_wait(pid, store);
  80146e:	83 ec 08             	sub    $0x8,%esp
  801471:	ff 75 0c             	pushl  0xc(%ebp)
  801474:	ff 75 08             	pushl  0x8(%ebp)
  801477:	e8 7a f7 ff ff       	call   800bf6 <sys_wait>
  80147c:	83 c4 10             	add    $0x10,%esp
}
  80147f:	c9                   	leave  
  801480:	c3                   	ret    

00801481 <yield>:

void yield(void)
{
  801481:	55                   	push   %ebp
  801482:	89 e5                	mov    %esp,%ebp
  801484:	83 ec 08             	sub    $0x8,%esp
    sys_yield();
  801487:	e8 7f f7 ff ff       	call   800c0b <sys_yield>
}
  80148c:	90                   	nop
  80148d:	c9                   	leave  
  80148e:	c3                   	ret    

0080148f <kill>:

int kill(int pid)
{
  80148f:	55                   	push   %ebp
  801490:	89 e5                	mov    %esp,%ebp
  801492:	83 ec 08             	sub    $0x8,%esp
    return sys_kill(pid);
  801495:	83 ec 0c             	sub    $0xc,%esp
  801498:	ff 75 08             	pushl  0x8(%ebp)
  80149b:	e8 7a f7 ff ff       	call   800c1a <sys_kill>
  8014a0:	83 c4 10             	add    $0x10,%esp
}
  8014a3:	c9                   	leave  
  8014a4:	c3                   	ret    

008014a5 <getpid>:

int getpid(void)
{
  8014a5:	55                   	push   %ebp
  8014a6:	89 e5                	mov    %esp,%ebp
  8014a8:	83 ec 08             	sub    $0x8,%esp
    return sys_getpid();
  8014ab:	e8 7c f7 ff ff       	call   800c2c <sys_getpid>
}
  8014b0:	c9                   	leave  
  8014b1:	c3                   	ret    

008014b2 <print_pgdir>:

//print_pgdir - print the PDT&PT
void print_pgdir(void)
{
  8014b2:	55                   	push   %ebp
  8014b3:	89 e5                	mov    %esp,%ebp
  8014b5:	83 ec 08             	sub    $0x8,%esp
    sys_pgdir();
  8014b8:	e8 ad f7 ff ff       	call   800c6a <sys_pgdir>
}
  8014bd:	90                   	nop
  8014be:	c9                   	leave  
  8014bf:	c3                   	ret    

008014c0 <print_vm>:

void print_vm(void)
{
  8014c0:	55                   	push   %ebp
  8014c1:	89 e5                	mov    %esp,%ebp
  8014c3:	83 ec 08             	sub    $0x8,%esp
    sys_pvm();
  8014c6:	e8 ae f7 ff ff       	call   800c79 <sys_pvm>
}
  8014cb:	90                   	nop
  8014cc:	c9                   	leave  
  8014cd:	c3                   	ret    

008014ce <print_vfs>:

void print_vfs(void)
{
  8014ce:	55                   	push   %ebp
  8014cf:	89 e5                	mov    %esp,%ebp
  8014d1:	83 ec 08             	sub    $0x8,%esp
    sys_pvfs();
  8014d4:	e8 af f7 ff ff       	call   800c88 <sys_pvfs>
}
  8014d9:	90                   	nop
  8014da:	c9                   	leave  
  8014db:	c3                   	ret    

008014dc <mmap>:

int mmap(uintptr_t *addr_store, size_t len, uint32_t mmap_flags)
{
  8014dc:	55                   	push   %ebp
  8014dd:	89 e5                	mov    %esp,%ebp
  8014df:	83 ec 08             	sub    $0x8,%esp
    return sys_mmap(addr_store, len, mmap_flags);
  8014e2:	83 ec 04             	sub    $0x4,%esp
  8014e5:	ff 75 10             	pushl  0x10(%ebp)
  8014e8:	ff 75 0c             	pushl  0xc(%ebp)
  8014eb:	ff 75 08             	pushl  0x8(%ebp)
  8014ee:	e8 ed f7 ff ff       	call   800ce0 <sys_mmap>
  8014f3:	83 c4 10             	add    $0x10,%esp
}
  8014f6:	c9                   	leave  
  8014f7:	c3                   	ret    

008014f8 <munmap>:

int munmap(uintptr_t addr, size_t len)
{
  8014f8:	55                   	push   %ebp
  8014f9:	89 e5                	mov    %esp,%ebp
  8014fb:	83 ec 08             	sub    $0x8,%esp
    return sys_munmap(addr, len);
  8014fe:	83 ec 08             	sub    $0x8,%esp
  801501:	ff 75 0c             	pushl  0xc(%ebp)
  801504:	ff 75 08             	pushl  0x8(%ebp)
  801507:	e8 ec f7 ff ff       	call   800cf8 <sys_munmap>
  80150c:	83 c4 10             	add    $0x10,%esp
}
  80150f:	c9                   	leave  
  801510:	c3                   	ret    

00801511 <set_priority>:

void set_priority(uint32_t priority)
{
  801511:	55                   	push   %ebp
  801512:	89 e5                	mov    %esp,%ebp
  801514:	83 ec 08             	sub    $0x8,%esp
    sys_set_priority(priority);
  801517:	83 ec 0c             	sub    $0xc,%esp
  80151a:	ff 75 08             	pushl  0x8(%ebp)
  80151d:	e8 75 f7 ff ff       	call   800c97 <sys_set_priority>
  801522:	83 c4 10             	add    $0x10,%esp
}
  801525:	90                   	nop
  801526:	c9                   	leave  
  801527:	c3                   	ret    

00801528 <sleep>:

int sleep(unsigned int time)
{
  801528:	55                   	push   %ebp
  801529:	89 e5                	mov    %esp,%ebp
  80152b:	83 ec 08             	sub    $0x8,%esp
    return sys_sleep(time);
  80152e:	83 ec 0c             	sub    $0xc,%esp
  801531:	ff 75 08             	pushl  0x8(%ebp)
  801534:	e8 74 f7 ff ff       	call   800cad <sys_sleep>
  801539:	83 c4 10             	add    $0x10,%esp
}
  80153c:	c9                   	leave  
  80153d:	c3                   	ret    

0080153e <gettime_msec>:

unsigned int gettime_msec(void)
{
  80153e:	55                   	push   %ebp
  80153f:	89 e5                	mov    %esp,%ebp
  801541:	83 ec 08             	sub    $0x8,%esp
    return (unsigned int)sys_gettime();
  801544:	e8 76 f7 ff ff       	call   800cbf <sys_gettime>
}
  801549:	c9                   	leave  
  80154a:	c3                   	ret    

0080154b <send_event>:

int send_event(int pid, int event_type, int event)
{
  80154b:	55                   	push   %ebp
  80154c:	89 e5                	mov    %esp,%ebp
  80154e:	83 ec 08             	sub    $0x8,%esp
    return sys_send_event(pid, event_type, event);
  801551:	83 ec 04             	sub    $0x4,%esp
  801554:	ff 75 10             	pushl  0x10(%ebp)
  801557:	ff 75 0c             	pushl  0xc(%ebp)
  80155a:	ff 75 08             	pushl  0x8(%ebp)
  80155d:	e8 2d fa ff ff       	call   800f8f <sys_send_event>
  801562:	83 c4 10             	add    $0x10,%esp
}
  801565:	c9                   	leave  
  801566:	c3                   	ret    

00801567 <recv_event>:

int recv_event(int *pid_store, int event_type, int *event_store)
{
  801567:	55                   	push   %ebp
  801568:	89 e5                	mov    %esp,%ebp
  80156a:	83 ec 08             	sub    $0x8,%esp
    return sys_recv_event(pid_store, event_type, event_store, 0);
  80156d:	6a 00                	push   $0x0
  80156f:	ff 75 10             	pushl  0x10(%ebp)
  801572:	ff 75 0c             	pushl  0xc(%ebp)
  801575:	ff 75 08             	pushl  0x8(%ebp)
  801578:	e8 2e fa ff ff       	call   800fab <sys_recv_event>
  80157d:	83 c4 10             	add    $0x10,%esp
}
  801580:	c9                   	leave  
  801581:	c3                   	ret    

00801582 <recv_event_timeout>:

int recv_event_timeout(int *pid_store, int event_type, int *event_store, unsigned int timeout)
{
  801582:	55                   	push   %ebp
  801583:	89 e5                	mov    %esp,%ebp
  801585:	83 ec 08             	sub    $0x8,%esp
    return sys_recv_event(pid_store, event_type, event_store, timeout);
  801588:	ff 75 14             	pushl  0x14(%ebp)
  80158b:	ff 75 10             	pushl  0x10(%ebp)
  80158e:	ff 75 0c             	pushl  0xc(%ebp)
  801591:	ff 75 08             	pushl  0x8(%ebp)
  801594:	e8 12 fa ff ff       	call   800fab <sys_recv_event>
  801599:	83 c4 10             	add    $0x10,%esp
}
  80159c:	c9                   	leave  
  80159d:	c3                   	ret    

0080159e <mbox_init>:

// 这个是调用 new_mbox,名称不好
int mbox_init(unsigned int max_slots)
{
  80159e:	55                   	push   %ebp
  80159f:	89 e5                	mov    %esp,%ebp
  8015a1:	83 ec 08             	sub    $0x8,%esp
    return sys_mbox_init(max_slots);
  8015a4:	83 ec 0c             	sub    $0xc,%esp
  8015a7:	ff 75 08             	pushl  0x8(%ebp)
  8015aa:	e8 1d fa ff ff       	call   800fcc <sys_mbox_init>
  8015af:	83 c4 10             	add    $0x10,%esp
}
  8015b2:	c9                   	leave  
  8015b3:	c3                   	ret    

008015b4 <mbox_send>:

int mbox_send(int id, struct mboxbuf *buf)
{
  8015b4:	55                   	push   %ebp
  8015b5:	89 e5                	mov    %esp,%ebp
  8015b7:	83 ec 08             	sub    $0x8,%esp
    return sys_mbox_send(id, buf, 0);
  8015ba:	83 ec 04             	sub    $0x4,%esp
  8015bd:	6a 00                	push   $0x0
  8015bf:	ff 75 0c             	pushl  0xc(%ebp)
  8015c2:	ff 75 08             	pushl  0x8(%ebp)
  8015c5:	e8 1c fa ff ff       	call   800fe6 <sys_mbox_send>
  8015ca:	83 c4 10             	add    $0x10,%esp
}
  8015cd:	c9                   	leave  
  8015ce:	c3                   	ret    

008015cf <mbox_send_timeout>:

int mbox_send_timeout(int id, struct mboxbuf *buf, unsigned int timeout)
{
  8015cf:	55                   	push   %ebp
  8015d0:	89 e5                	mov    %esp,%ebp
  8015d2:	83 ec 08             	sub    $0x8,%esp
    return sys_mbox_send(id, buf, timeout);
  8015d5:	83 ec 04             	sub    $0x4,%esp
  8015d8:	ff 75 10             	pushl  0x10(%ebp)
  8015db:	ff 75 0c             	pushl  0xc(%ebp)
  8015de:	ff 75 08             	pushl  0x8(%ebp)
  8015e1:	e8 00 fa ff ff       	call   800fe6 <sys_mbox_send>
  8015e6:	83 c4 10             	add    $0x10,%esp
}
  8015e9:	c9                   	leave  
  8015ea:	c3                   	ret    

008015eb <mbox_recv>:

int mbox_recv(int id, struct mboxbuf *buf)
{
  8015eb:	55                   	push   %ebp
  8015ec:	89 e5                	mov    %esp,%ebp
  8015ee:	83 ec 08             	sub    $0x8,%esp
    return sys_mbox_recv(id, buf, 0);
  8015f1:	83 ec 04             	sub    $0x4,%esp
  8015f4:	6a 00                	push   $0x0
  8015f6:	ff 75 0c             	pushl  0xc(%ebp)
  8015f9:	ff 75 08             	pushl  0x8(%ebp)
  8015fc:	e8 02 fa ff ff       	call   801003 <sys_mbox_recv>
  801601:	83 c4 10             	add    $0x10,%esp
}
  801604:	c9                   	leave  
  801605:	c3                   	ret    

00801606 <mbox_recv_timeout>:

int mbox_recv_timeout(int id, struct mboxbuf *buf, unsigned int timeout)
{
  801606:	55                   	push   %ebp
  801607:	89 e5                	mov    %esp,%ebp
  801609:	83 ec 08             	sub    $0x8,%esp
    return sys_mbox_recv(id, buf, timeout);
  80160c:	83 ec 04             	sub    $0x4,%esp
  80160f:	ff 75 10             	pushl  0x10(%ebp)
  801612:	ff 75 0c             	pushl  0xc(%ebp)
  801615:	ff 75 08             	pushl  0x8(%ebp)
  801618:	e8 e6 f9 ff ff       	call   801003 <sys_mbox_recv>
  80161d:	83 c4 10             	add    $0x10,%esp
}
  801620:	c9                   	leave  
  801621:	c3                   	ret    

00801622 <mbox_free>:

int mbox_free(int id)
{
  801622:	55                   	push   %ebp
  801623:	89 e5                	mov    %esp,%ebp
  801625:	83 ec 08             	sub    $0x8,%esp
    return sys_mbox_free(id);
  801628:	83 ec 0c             	sub    $0xc,%esp
  80162b:	ff 75 08             	pushl  0x8(%ebp)
  80162e:	e8 ed f9 ff ff       	call   801020 <sys_mbox_free>
  801633:	83 c4 10             	add    $0x10,%esp
}
  801636:	c9                   	leave  
  801637:	c3                   	ret    

00801638 <mbox_info>:

int mbox_info(int id, struct mboxinfo *info)
{
  801638:	55                   	push   %ebp
  801639:	89 e5                	mov    %esp,%ebp
  80163b:	83 ec 08             	sub    $0x8,%esp
    return sys_mbox_info(id, info);
  80163e:	83 ec 08             	sub    $0x8,%esp
  801641:	ff 75 0c             	pushl  0xc(%ebp)
  801644:	ff 75 08             	pushl  0x8(%ebp)
  801647:	e8 ee f9 ff ff       	call   80103a <sys_mbox_info>
  80164c:	83 c4 10             	add    $0x10,%esp
}
  80164f:	c9                   	leave  
  801650:	c3                   	ret    

00801651 <receive_packet>:

int receive_packet(uint8_t *buf, size_t len)
{
  801651:	55                   	push   %ebp
  801652:	89 e5                	mov    %esp,%ebp
  801654:	83 ec 18             	sub    $0x18,%esp
    size_t len_store;
    //cprintf("user mode receive_packet len =%x %d\n",len,len);
    sys_receive_packet(buf ,len ,&len_store);
  801657:	83 ec 04             	sub    $0x4,%esp
  80165a:	8d 45 f4             	lea    -0xc(%ebp),%eax
  80165d:	50                   	push   %eax
  80165e:	ff 75 0c             	pushl  0xc(%ebp)
  801661:	ff 75 08             	pushl  0x8(%ebp)
  801664:	e8 e4 f8 ff ff       	call   800f4d <sys_receive_packet>
  801669:	83 c4 10             	add    $0x10,%esp
    return (int)len_store;
  80166c:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
  80166f:	c9                   	leave  
  801670:	c3                   	ret    

00801671 <transmit_packet>:

int transmit_packet(uint8_t *buf, size_t len, size_t* len_store)
{
  801671:	55                   	push   %ebp
  801672:	89 e5                	mov    %esp,%ebp
  801674:	83 ec 08             	sub    $0x8,%esp
    return sys_transmit_packet(buf, len, len_store);
  801677:	83 ec 04             	sub    $0x4,%esp
  80167a:	ff 75 10             	pushl  0x10(%ebp)
  80167d:	ff 75 0c             	pushl  0xc(%ebp)
  801680:	ff 75 08             	pushl  0x8(%ebp)
  801683:	e8 e6 f8 ff ff       	call   800f6e <sys_transmit_packet>
  801688:	83 c4 10             	add    $0x10,%esp
}
  80168b:	c9                   	leave  
  80168c:	c3                   	ret    

0080168d <ping>:

int ping(char *target, int len)
{
  80168d:	55                   	push   %ebp
  80168e:	89 e5                	mov    %esp,%ebp
  801690:	83 ec 08             	sub    $0x8,%esp
    return sys_ping(target, len);
  801693:	83 ec 08             	sub    $0x8,%esp
  801696:	ff 75 0c             	pushl  0xc(%ebp)
  801699:	ff 75 08             	pushl  0x8(%ebp)
  80169c:	e8 b5 f9 ff ff       	call   801056 <sys_ping>
  8016a1:	83 c4 10             	add    $0x10,%esp
}
  8016a4:	c9                   	leave  
  8016a5:	c3                   	ret    

008016a6 <process_dump>:

int process_dump()
{
  8016a6:	55                   	push   %ebp
  8016a7:	89 e5                	mov    %esp,%ebp
  8016a9:	83 ec 08             	sub    $0x8,%esp
    return sys_process_dump();
  8016ac:	e8 c0 f9 ff ff       	call   801071 <sys_process_dump>
}
  8016b1:	c9                   	leave  
  8016b2:	c3                   	ret    

008016b3 <rtdump>:

int rtdump()
{
  8016b3:	55                   	push   %ebp
  8016b4:	89 e5                	mov    %esp,%ebp
  8016b6:	83 ec 08             	sub    $0x8,%esp
    return sys_rtdump();
  8016b9:	e8 cb f9 ff ff       	call   801089 <sys_rtdump>
}
  8016be:	c9                   	leave  
  8016bf:	c3                   	ret    

008016c0 <arpprint>:

int arpprint()
{
  8016c0:	55                   	push   %ebp
  8016c1:	89 e5                	mov    %esp,%ebp
  8016c3:	83 ec 08             	sub    $0x8,%esp
    return sys_arpprint();
  8016c6:	e8 d6 f9 ff ff       	call   8010a1 <sys_arpprint>
}
  8016cb:	c9                   	leave  
  8016cc:	c3                   	ret    

008016cd <netstatus>:

int netstatus()
{
  8016cd:	55                   	push   %ebp
  8016ce:	89 e5                	mov    %esp,%ebp
  8016d0:	83 ec 08             	sub    $0x8,%esp
    return sys_netstatus();
  8016d3:	e8 e1 f9 ff ff       	call   8010b9 <sys_netstatus>
}
  8016d8:	c9                   	leave  
  8016d9:	c3                   	ret    

008016da <sock_socket>:

int sock_socket(uint32_t type, const char* ipaddr, uint32_t iplen)
{
  8016da:	55                   	push   %ebp
  8016db:	89 e5                	mov    %esp,%ebp
  8016dd:	83 ec 08             	sub    $0x8,%esp
    return sys_sock_socket(type, ipaddr, iplen);
  8016e0:	83 ec 04             	sub    $0x4,%esp
  8016e3:	ff 75 10             	pushl  0x10(%ebp)
  8016e6:	ff 75 0c             	pushl  0xc(%ebp)
  8016e9:	ff 75 08             	pushl  0x8(%ebp)
  8016ec:	e8 e0 f9 ff ff       	call   8010d1 <sys_sock_socket>
  8016f1:	83 c4 10             	add    $0x10,%esp
}
  8016f4:	c9                   	leave  
  8016f5:	c3                   	ret    

008016f6 <sock_listen>:

int sock_listen(uint32_t tcpfd, uint32_t qsize)
{
  8016f6:	55                   	push   %ebp
  8016f7:	89 e5                	mov    %esp,%ebp
  8016f9:	83 ec 08             	sub    $0x8,%esp
    return sys_sock_listen(tcpfd, qsize);
  8016fc:	83 ec 08             	sub    $0x8,%esp
  8016ff:	ff 75 0c             	pushl  0xc(%ebp)
  801702:	ff 75 08             	pushl  0x8(%ebp)
  801705:	e8 e7 f9 ff ff       	call   8010f1 <sys_sock_listen>
  80170a:	83 c4 10             	add    $0x10,%esp
}
  80170d:	c9                   	leave  
  80170e:	c3                   	ret    

0080170f <sock_accept>:

int sock_accept(uint32_t listenfd, uint32_t timeout)
{
  80170f:	55                   	push   %ebp
  801710:	89 e5                	mov    %esp,%ebp
  801712:	83 ec 08             	sub    $0x8,%esp
    return sys_sock_accept(listenfd, timeout);
  801715:	83 ec 08             	sub    $0x8,%esp
  801718:	ff 75 0c             	pushl  0xc(%ebp)
  80171b:	ff 75 08             	pushl  0x8(%ebp)
  80171e:	e8 e8 f9 ff ff       	call   80110b <sys_sock_accept>
  801723:	83 c4 10             	add    $0x10,%esp
}
  801726:	c9                   	leave  
  801727:	c3                   	ret    

00801728 <sock_connect>:

int sock_connect(uint32_t sockfd, const char* ipaddr, uint32_t iplen)
{
  801728:	55                   	push   %ebp
  801729:	89 e5                	mov    %esp,%ebp
  80172b:	83 ec 08             	sub    $0x8,%esp
    return sys_sock_connect(sockfd, ipaddr, iplen);
  80172e:	83 ec 04             	sub    $0x4,%esp
  801731:	ff 75 10             	pushl  0x10(%ebp)
  801734:	ff 75 0c             	pushl  0xc(%ebp)
  801737:	ff 75 08             	pushl  0x8(%ebp)
  80173a:	e8 e6 f9 ff ff       	call   801125 <sys_sock_connect>
  80173f:	83 c4 10             	add    $0x10,%esp
}
  801742:	c9                   	leave  
  801743:	c3                   	ret    

00801744 <sock_bind>:

int sock_bind(uint32_t sockfd, uint32_t lport, uint32_t rport)
{
  801744:	55                   	push   %ebp
  801745:	89 e5                	mov    %esp,%ebp
  801747:	83 ec 08             	sub    $0x8,%esp
    return sys_sock_bind(sockfd, lport, rport);
  80174a:	83 ec 04             	sub    $0x4,%esp
  80174d:	ff 75 10             	pushl  0x10(%ebp)
  801750:	ff 75 0c             	pushl  0xc(%ebp)
  801753:	ff 75 08             	pushl  0x8(%ebp)
  801756:	e8 ea f9 ff ff       	call   801145 <sys_sock_bind>
  80175b:	83 c4 10             	add    $0x10,%esp
}
  80175e:	c9                   	leave  
  80175f:	c3                   	ret    

00801760 <sock_send>:

int sock_send(uint32_t sockfd, char* buf, uint32_t len, uint32_t timeout)
{
  801760:	55                   	push   %ebp
  801761:	89 e5                	mov    %esp,%ebp
  801763:	83 ec 08             	sub    $0x8,%esp
    return sys_sock_send(sockfd, buf, len, timeout);
  801766:	ff 75 14             	pushl  0x14(%ebp)
  801769:	ff 75 10             	pushl  0x10(%ebp)
  80176c:	ff 75 0c             	pushl  0xc(%ebp)
  80176f:	ff 75 08             	pushl  0x8(%ebp)
  801772:	e8 ed f9 ff ff       	call   801164 <sys_sock_send>
  801777:	83 c4 10             	add    $0x10,%esp
}
  80177a:	c9                   	leave  
  80177b:	c3                   	ret    

0080177c <sock_recv>:

int sock_recv(uint32_t sockfd, char* buf, uint32_t len, uint32_t timeout)
{
  80177c:	55                   	push   %ebp
  80177d:	89 e5                	mov    %esp,%ebp
  80177f:	83 ec 08             	sub    $0x8,%esp
    return sys_sock_recv(sockfd, buf, len, timeout);
  801782:	ff 75 14             	pushl  0x14(%ebp)
  801785:	ff 75 10             	pushl  0x10(%ebp)
  801788:	ff 75 0c             	pushl  0xc(%ebp)
  80178b:	ff 75 08             	pushl  0x8(%ebp)
  80178e:	e8 f2 f9 ff ff       	call   801185 <sys_sock_recv>
  801793:	83 c4 10             	add    $0x10,%esp
}
  801796:	c9                   	leave  
  801797:	c3                   	ret    

00801798 <sock_close>:

int sock_close(uint32_t sockfd)
{
  801798:	55                   	push   %ebp
  801799:	89 e5                	mov    %esp,%ebp
  80179b:	83 ec 08             	sub    $0x8,%esp
    return sys_sock_close(sockfd);
  80179e:	83 ec 0c             	sub    $0xc,%esp
  8017a1:	ff 75 08             	pushl  0x8(%ebp)
  8017a4:	e8 fd f9 ff ff       	call   8011a6 <sys_sock_close>
  8017a9:	83 c4 10             	add    $0x10,%esp
}
  8017ac:	c9                   	leave  
  8017ad:	c3                   	ret    

008017ae <sock_shutdown>:

int sock_shutdown(uint32_t sockfd, uint32_t type)
{
  8017ae:	55                   	push   %ebp
  8017af:	89 e5                	mov    %esp,%ebp
  8017b1:	83 ec 08             	sub    $0x8,%esp
    return sys_sock_shutdown(sockfd, type);
  8017b4:	83 ec 08             	sub    $0x8,%esp
  8017b7:	ff 75 0c             	pushl  0xc(%ebp)
  8017ba:	ff 75 08             	pushl  0x8(%ebp)
  8017bd:	e8 fd f9 ff ff       	call   8011bf <sys_sock_shutdown>
  8017c2:	83 c4 10             	add    $0x10,%esp
}
  8017c5:	c9                   	leave  
  8017c6:	c3                   	ret    

008017c7 <__exec>:

int __exec(const char *name, const char **argv)
{
  8017c7:	55                   	push   %ebp
  8017c8:	89 e5                	mov    %esp,%ebp
  8017ca:	83 ec 18             	sub    $0x18,%esp
    int argc = 0;
  8017cd:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
    while (argv[argc] != NULL)
  8017d4:	eb 03                	jmp    8017d9 <__exec+0x12>
    {
        argc ++;
  8017d6:	ff 45 f4             	incl   -0xc(%ebp)
    while (argv[argc] != NULL)
  8017d9:	8b 45 f4             	mov    -0xc(%ebp),%eax
  8017dc:	8d 14 85 00 00 00 00 	lea    0x0(,%eax,4),%edx
  8017e3:	8b 45 0c             	mov    0xc(%ebp),%eax
  8017e6:	01 d0                	add    %edx,%eax
  8017e8:	8b 00                	mov    (%eax),%eax
  8017ea:	85 c0                	test   %eax,%eax
  8017ec:	75 e8                	jne    8017d6 <__exec+0xf>
    }
    return sys_exec(name, argc, argv);
  8017ee:	83 ec 04             	sub    $0x4,%esp
  8017f1:	ff 75 0c             	pushl  0xc(%ebp)
  8017f4:	ff 75 f4             	pushl  -0xc(%ebp)
  8017f7:	ff 75 08             	pushl  0x8(%ebp)
  8017fa:	e8 0e f5 ff ff       	call   800d0d <sys_exec>
  8017ff:	83 c4 10             	add    $0x10,%esp
}
  801802:	c9                   	leave  
  801803:	c3                   	ret    

00801804 <halt>:

void halt()
{
  801804:	55                   	push   %ebp
  801805:	89 e5                	mov    %esp,%ebp
//    sys_halt();
}
  801807:	90                   	nop
  801808:	5d                   	pop    %ebp
  801809:	c3                   	ret    

0080180a <initfd>:
#include "stdio.h"

int main(int argc, char **argv);

static int initfd(int fd2, const char *path, uint32_t open_flags)
{
  80180a:	55                   	push   %ebp
  80180b:	89 e5                	mov    %esp,%ebp
  80180d:	83 ec 28             	sub    $0x28,%esp
    struct stat __stat, *stat = &__stat;
  801810:	8d 45 dc             	lea    -0x24(%ebp),%eax
  801813:	89 45 f0             	mov    %eax,-0x10(%ebp)
    int ret, fd1;
    
    // 如果 fd 已经打开过了，就不要再打开了
    if ((ret = fstat(fd2, stat)) != 0)
  801816:	83 ec 08             	sub    $0x8,%esp
  801819:	ff 75 f0             	pushl  -0x10(%ebp)
  80181c:	ff 75 08             	pushl  0x8(%ebp)
  80181f:	e8 19 ea ff ff       	call   80023d <fstat>
  801824:	83 c4 10             	add    $0x10,%esp
  801827:	89 45 f4             	mov    %eax,-0xc(%ebp)
  80182a:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
  80182e:	74 57                	je     801887 <initfd+0x7d>
    {
        if ((fd1 = open(path, open_flags)) < 0 || fd1 == fd2)
  801830:	83 ec 08             	sub    $0x8,%esp
  801833:	ff 75 10             	pushl  0x10(%ebp)
  801836:	ff 75 0c             	pushl  0xc(%ebp)
  801839:	e8 7a e9 ff ff       	call   8001b8 <open>
  80183e:	83 c4 10             	add    $0x10,%esp
  801841:	89 45 ec             	mov    %eax,-0x14(%ebp)
  801844:	83 7d ec 00          	cmpl   $0x0,-0x14(%ebp)
  801848:	78 08                	js     801852 <initfd+0x48>
  80184a:	8b 45 ec             	mov    -0x14(%ebp),%eax
  80184d:	3b 45 08             	cmp    0x8(%ebp),%eax
  801850:	75 05                	jne    801857 <initfd+0x4d>
        {
            return fd1;
  801852:	8b 45 ec             	mov    -0x14(%ebp),%eax
  801855:	eb 33                	jmp    80188a <initfd+0x80>
        }
        close(fd2);
  801857:	83 ec 0c             	sub    $0xc,%esp
  80185a:	ff 75 08             	pushl  0x8(%ebp)
  80185d:	e8 71 e9 ff ff       	call   8001d3 <close>
  801862:	83 c4 10             	add    $0x10,%esp
        ret = dup2(fd1, fd2);
  801865:	83 ec 08             	sub    $0x8,%esp
  801868:	ff 75 08             	pushl  0x8(%ebp)
  80186b:	ff 75 ec             	pushl  -0x14(%ebp)
  80186e:	e8 f9 e9 ff ff       	call   80026c <dup2>
  801873:	83 c4 10             	add    $0x10,%esp
  801876:	89 45 f4             	mov    %eax,-0xc(%ebp)
        close(fd1);
  801879:	83 ec 0c             	sub    $0xc,%esp
  80187c:	ff 75 ec             	pushl  -0x14(%ebp)
  80187f:	e8 4f e9 ff ff       	call   8001d3 <close>
  801884:	83 c4 10             	add    $0x10,%esp
    }
    
    return ret;
  801887:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
  80188a:	c9                   	leave  
  80188b:	c3                   	ret    

0080188c <umain>:

void umain(int argc, char **argv)
{
  80188c:	55                   	push   %ebp
  80188d:	89 e5                	mov    %esp,%ebp
  80188f:	83 ec 18             	sub    $0x18,%esp
    int fd;
    
    cprintf("\n-------------------- umain start --------------------\n");
  801892:	83 ec 0c             	sub    $0xc,%esp
  801895:	68 4c 29 80 00       	push   $0x80294c
  80189a:	e8 e3 f1 ff ff       	call   800a82 <cprintf>
  80189f:	83 c4 10             	add    $0x10,%esp
    print_pgdir();
  8018a2:	e8 0b fc ff ff       	call   8014b2 <print_pgdir>
    print_vm();
  8018a7:	e8 14 fc ff ff       	call   8014c0 <print_vm>
    print_vfs();
  8018ac:	e8 1d fc ff ff       	call   8014ce <print_vfs>
    
    // 用户程序运行一开始就把 stdin 和 stdout 打开，并映射到 0 和 1 两个文件描述符上
    if ((fd = initfd(0, "stdin:", O_RDONLY)) < 0)
  8018b1:	83 ec 04             	sub    $0x4,%esp
  8018b4:	6a 00                	push   $0x0
  8018b6:	68 84 29 80 00       	push   $0x802984
  8018bb:	6a 00                	push   $0x0
  8018bd:	e8 48 ff ff ff       	call   80180a <initfd>
  8018c2:	83 c4 10             	add    $0x10,%esp
  8018c5:	89 45 f4             	mov    %eax,-0xc(%ebp)
  8018c8:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
  8018cc:	79 17                	jns    8018e5 <umain+0x59>
    {
        warn("open <stdin> failed: %e.\n", fd);
  8018ce:	ff 75 f4             	pushl  -0xc(%ebp)
  8018d1:	68 8b 29 80 00       	push   $0x80298b
  8018d6:	6a 29                	push   $0x29
  8018d8:	68 a5 29 80 00       	push   $0x8029a5
  8018dd:	e8 12 f0 ff ff       	call   8008f4 <__warn>
  8018e2:	83 c4 10             	add    $0x10,%esp
    }
    
    if ((fd = initfd(1, "stdout:", O_WRONLY)) < 0)
  8018e5:	83 ec 04             	sub    $0x4,%esp
  8018e8:	6a 01                	push   $0x1
  8018ea:	68 b7 29 80 00       	push   $0x8029b7
  8018ef:	6a 01                	push   $0x1
  8018f1:	e8 14 ff ff ff       	call   80180a <initfd>
  8018f6:	83 c4 10             	add    $0x10,%esp
  8018f9:	89 45 f4             	mov    %eax,-0xc(%ebp)
  8018fc:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
  801900:	79 17                	jns    801919 <umain+0x8d>
    {
        warn("open <stdout> failed: %e.\n", fd);
  801902:	ff 75 f4             	pushl  -0xc(%ebp)
  801905:	68 bf 29 80 00       	push   $0x8029bf
  80190a:	6a 2e                	push   $0x2e
  80190c:	68 a5 29 80 00       	push   $0x8029a5
  801911:	e8 de ef ff ff       	call   8008f4 <__warn>
  801916:	83 c4 10             	add    $0x10,%esp
    }
    
    if ((fd = initfd(2, "stdout:", O_WRONLY)) < 0)
  801919:	83 ec 04             	sub    $0x4,%esp
  80191c:	6a 01                	push   $0x1
  80191e:	68 b7 29 80 00       	push   $0x8029b7
  801923:	6a 02                	push   $0x2
  801925:	e8 e0 fe ff ff       	call   80180a <initfd>
  80192a:	83 c4 10             	add    $0x10,%esp
  80192d:	89 45 f4             	mov    %eax,-0xc(%ebp)
  801930:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
  801934:	79 17                	jns    80194d <umain+0xc1>
    {
        warn("open <stderr> failed: %e.\n", fd);
  801936:	ff 75 f4             	pushl  -0xc(%ebp)
  801939:	68 da 29 80 00       	push   $0x8029da
  80193e:	6a 33                	push   $0x33
  801940:	68 a5 29 80 00       	push   $0x8029a5
  801945:	e8 aa ef ff ff       	call   8008f4 <__warn>
  80194a:	83 c4 10             	add    $0x10,%esp
    }
    
    int ret = main(argc, argv);
  80194d:	83 ec 08             	sub    $0x8,%esp
  801950:	ff 75 0c             	pushl  0xc(%ebp)
  801953:	ff 75 08             	pushl  0x8(%ebp)
  801956:	e8 9e 0e 00 00       	call   8027f9 <main>
  80195b:	83 c4 10             	add    $0x10,%esp
  80195e:	89 45 f0             	mov    %eax,-0x10(%ebp)
    cprintf("\n-------------------- umain end --------------------\n");
  801961:	83 ec 0c             	sub    $0xc,%esp
  801964:	68 f8 29 80 00       	push   $0x8029f8
  801969:	e8 14 f1 ff ff       	call   800a82 <cprintf>
  80196e:	83 c4 10             	add    $0x10,%esp
    exit(ret);    
  801971:	83 ec 0c             	sub    $0xc,%esp
  801974:	ff 75 f0             	pushl  -0x10(%ebp)
  801977:	e8 6d fa ff ff       	call   8013e9 <exit>

0080197c <hash32>:
 * @bits:   the number of bits in a return value
 *
 * High bits are more random, so we use them.
 * */
uint32_t hash32(uint32_t val, unsigned int bits)
{
  80197c:	55                   	push   %ebp
  80197d:	89 e5                	mov    %esp,%ebp
  80197f:	83 ec 10             	sub    $0x10,%esp
    uint32_t hash = val * GOLDEN_RATIO_PRIME_32;
  801982:	8b 55 08             	mov    0x8(%ebp),%edx
  801985:	89 d0                	mov    %edx,%eax
  801987:	01 c0                	add    %eax,%eax
  801989:	01 d0                	add    %edx,%eax
  80198b:	89 c1                	mov    %eax,%ecx
  80198d:	c1 e1 06             	shl    $0x6,%ecx
  801990:	01 c8                	add    %ecx,%eax
  801992:	01 c0                	add    %eax,%eax
  801994:	01 d0                	add    %edx,%eax
  801996:	c1 e0 03             	shl    $0x3,%eax
  801999:	01 d0                	add    %edx,%eax
  80199b:	c1 e0 03             	shl    $0x3,%eax
  80199e:	01 d0                	add    %edx,%eax
  8019a0:	c1 e0 10             	shl    $0x10,%eax
  8019a3:	29 d0                	sub    %edx,%eax
  8019a5:	89 45 fc             	mov    %eax,-0x4(%ebp)
  8019a8:	f7 5d fc             	negl   -0x4(%ebp)
    return (hash >> (32 - bits));
  8019ab:	b8 20 00 00 00       	mov    $0x20,%eax
  8019b0:	2b 45 0c             	sub    0xc(%ebp),%eax
  8019b3:	8b 55 fc             	mov    -0x4(%ebp),%edx
  8019b6:	88 c1                	mov    %al,%cl
  8019b8:	d3 ea                	shr    %cl,%edx
  8019ba:	89 d0                	mov    %edx,%eax
}
  8019bc:	c9                   	leave  
  8019bd:	c3                   	ret    
  8019be:	66 90                	xchg   %ax,%ax

008019c0 <setjmp>:
#define ENTRY(x) \
        .text; _ALIGN_TEXT; .globl x; .type x,@function; x:


ENTRY(setjmp)
	movl	4(%esp), %ecx	// jmp_buf
  8019c0:	8b 4c 24 04          	mov    0x4(%esp),%ecx

	movl	0(%esp), %edx	// %eip as pushed by call
  8019c4:	8b 14 24             	mov    (%esp),%edx
	movl	%edx,  0(%ecx)
  8019c7:	89 11                	mov    %edx,(%ecx)

	leal	4(%esp), %edx	// where %esp will point when we return
  8019c9:	8d 54 24 04          	lea    0x4(%esp),%edx
	movl	%edx,  4(%ecx)
  8019cd:	89 51 04             	mov    %edx,0x4(%ecx)

	movl	%ebp,  8(%ecx)
  8019d0:	89 69 08             	mov    %ebp,0x8(%ecx)
	movl	%ebx, 12(%ecx)
  8019d3:	89 59 0c             	mov    %ebx,0xc(%ecx)
	movl	%esi, 16(%ecx)
  8019d6:	89 71 10             	mov    %esi,0x10(%ecx)
	movl	%edi, 20(%ecx)
  8019d9:	89 79 14             	mov    %edi,0x14(%ecx)

	movl	$0, %eax
  8019dc:	b8 00 00 00 00       	mov    $0x0,%eax
	ret
  8019e1:	c3                   	ret    
  8019e2:	8d b4 26 00 00 00 00 	lea    0x0(%esi,%eiz,1),%esi
  8019e9:	8d b4 26 00 00 00 00 	lea    0x0(%esi,%eiz,1),%esi

008019f0 <longjmp>:

ENTRY(longjmp)
	// %eax is the jmp_buf*
	// %edx is the return value

	movl	 0(%eax), %ecx	// %eip
  8019f0:	8b 08                	mov    (%eax),%ecx
	movl	 4(%eax), %esp
  8019f2:	8b 60 04             	mov    0x4(%eax),%esp
	movl	 8(%eax), %ebp
  8019f5:	8b 68 08             	mov    0x8(%eax),%ebp
	movl	12(%eax), %ebx
  8019f8:	8b 58 0c             	mov    0xc(%eax),%ebx
	movl	16(%eax), %esi
  8019fb:	8b 70 10             	mov    0x10(%eax),%esi
	movl	20(%eax), %edi
  8019fe:	8b 78 14             	mov    0x14(%eax),%edi

	movl	%edx, %eax
  801a01:	89 d0                	mov    %edx,%eax
	jmp	    *%ecx
  801a03:	ff e1                	jmp    *%ecx

00801a05 <printnum>:
 * @width:      maximum number of digits, if the actual width is less than @width, use @padc instead
 * @padc:       character that padded on the left if the actual width is less than @width
 * */
static void printnum(void (*putch)(int, void*, int), int fd, void *putdat,
        unsigned long long num, unsigned base, int width, int padc)
{
  801a05:	55                   	push   %ebp
  801a06:	89 e5                	mov    %esp,%ebp
  801a08:	83 ec 38             	sub    $0x38,%esp
  801a0b:	8b 45 14             	mov    0x14(%ebp),%eax
  801a0e:	89 45 d0             	mov    %eax,-0x30(%ebp)
  801a11:	8b 45 18             	mov    0x18(%ebp),%eax
  801a14:	89 45 d4             	mov    %eax,-0x2c(%ebp)
    unsigned long long result = num;
  801a17:	8b 45 d0             	mov    -0x30(%ebp),%eax
  801a1a:	8b 55 d4             	mov    -0x2c(%ebp),%edx
  801a1d:	89 45 e8             	mov    %eax,-0x18(%ebp)
  801a20:	89 55 ec             	mov    %edx,-0x14(%ebp)
    unsigned long mod = do_div(result, base);
  801a23:	8b 45 1c             	mov    0x1c(%ebp),%eax
  801a26:	89 45 e4             	mov    %eax,-0x1c(%ebp)
  801a29:	8b 45 e8             	mov    -0x18(%ebp),%eax
  801a2c:	8b 55 ec             	mov    -0x14(%ebp),%edx
  801a2f:	89 45 e0             	mov    %eax,-0x20(%ebp)
  801a32:	89 55 f0             	mov    %edx,-0x10(%ebp)
  801a35:	8b 45 f0             	mov    -0x10(%ebp),%eax
  801a38:	89 45 f4             	mov    %eax,-0xc(%ebp)
  801a3b:	83 7d f0 00          	cmpl   $0x0,-0x10(%ebp)
  801a3f:	74 1c                	je     801a5d <printnum+0x58>
  801a41:	8b 45 f0             	mov    -0x10(%ebp),%eax
  801a44:	ba 00 00 00 00       	mov    $0x0,%edx
  801a49:	f7 75 e4             	divl   -0x1c(%ebp)
  801a4c:	89 55 f4             	mov    %edx,-0xc(%ebp)
  801a4f:	8b 45 f0             	mov    -0x10(%ebp),%eax
  801a52:	ba 00 00 00 00       	mov    $0x0,%edx
  801a57:	f7 75 e4             	divl   -0x1c(%ebp)
  801a5a:	89 45 f0             	mov    %eax,-0x10(%ebp)
  801a5d:	8b 45 e0             	mov    -0x20(%ebp),%eax
  801a60:	8b 55 f4             	mov    -0xc(%ebp),%edx
  801a63:	f7 75 e4             	divl   -0x1c(%ebp)
  801a66:	89 45 e0             	mov    %eax,-0x20(%ebp)
  801a69:	89 55 dc             	mov    %edx,-0x24(%ebp)
  801a6c:	8b 45 e0             	mov    -0x20(%ebp),%eax
  801a6f:	8b 55 f0             	mov    -0x10(%ebp),%edx
  801a72:	89 45 e8             	mov    %eax,-0x18(%ebp)
  801a75:	89 55 ec             	mov    %edx,-0x14(%ebp)
  801a78:	8b 45 dc             	mov    -0x24(%ebp),%eax
  801a7b:	89 45 d8             	mov    %eax,-0x28(%ebp)

    // first recursively print all preceding (more significant) digits
    if (num >= base) {
  801a7e:	8b 45 1c             	mov    0x1c(%ebp),%eax
  801a81:	ba 00 00 00 00       	mov    $0x0,%edx
  801a86:	8b 4d d4             	mov    -0x2c(%ebp),%ecx
  801a89:	39 45 d0             	cmp    %eax,-0x30(%ebp)
  801a8c:	19 d1                	sbb    %edx,%ecx
  801a8e:	72 38                	jb     801ac8 <printnum+0xc3>
        printnum(putch, fd, putdat, result, base, width - 1, padc);
  801a90:	8b 45 20             	mov    0x20(%ebp),%eax
  801a93:	48                   	dec    %eax
  801a94:	ff 75 24             	pushl  0x24(%ebp)
  801a97:	50                   	push   %eax
  801a98:	ff 75 1c             	pushl  0x1c(%ebp)
  801a9b:	ff 75 ec             	pushl  -0x14(%ebp)
  801a9e:	ff 75 e8             	pushl  -0x18(%ebp)
  801aa1:	ff 75 10             	pushl  0x10(%ebp)
  801aa4:	ff 75 0c             	pushl  0xc(%ebp)
  801aa7:	ff 75 08             	pushl  0x8(%ebp)
  801aaa:	e8 56 ff ff ff       	call   801a05 <printnum>
  801aaf:	83 c4 20             	add    $0x20,%esp
  801ab2:	eb 1d                	jmp    801ad1 <printnum+0xcc>
    } else {
        // print any needed pad characters before first digit
        while (-- width > 0)
            putch(padc, putdat, fd);
  801ab4:	83 ec 04             	sub    $0x4,%esp
  801ab7:	ff 75 0c             	pushl  0xc(%ebp)
  801aba:	ff 75 10             	pushl  0x10(%ebp)
  801abd:	ff 75 24             	pushl  0x24(%ebp)
  801ac0:	8b 45 08             	mov    0x8(%ebp),%eax
  801ac3:	ff d0                	call   *%eax
  801ac5:	83 c4 10             	add    $0x10,%esp
        while (-- width > 0)
  801ac8:	ff 4d 20             	decl   0x20(%ebp)
  801acb:	83 7d 20 00          	cmpl   $0x0,0x20(%ebp)
  801acf:	7f e3                	jg     801ab4 <printnum+0xaf>
    }
    // then print this (the least significant) digit
    putch("0123456789abcdef"[mod], putdat, fd);
  801ad1:	8b 45 d8             	mov    -0x28(%ebp),%eax
  801ad4:	05 58 2c 80 00       	add    $0x802c58,%eax
  801ad9:	8a 00                	mov    (%eax),%al
  801adb:	0f be c0             	movsbl %al,%eax
  801ade:	83 ec 04             	sub    $0x4,%esp
  801ae1:	ff 75 0c             	pushl  0xc(%ebp)
  801ae4:	ff 75 10             	pushl  0x10(%ebp)
  801ae7:	50                   	push   %eax
  801ae8:	8b 45 08             	mov    0x8(%ebp),%eax
  801aeb:	ff d0                	call   *%eax
  801aed:	83 c4 10             	add    $0x10,%esp
}
  801af0:	90                   	nop
  801af1:	c9                   	leave  
  801af2:	c3                   	ret    

00801af3 <getuint>:
 * getuint - get an unsigned int of various possible sizes from a varargs list
 * @ap:         a varargs list pointer
 * @lflag:      determines the size of the vararg that @ap points to
 * */
static unsigned long long getuint(va_list *ap, int lflag)
{
  801af3:	55                   	push   %ebp
  801af4:	89 e5                	mov    %esp,%ebp
    if (lflag >= 2) {
  801af6:	83 7d 0c 01          	cmpl   $0x1,0xc(%ebp)
  801afa:	7e 14                	jle    801b10 <getuint+0x1d>
        return va_arg(*ap, unsigned long long);
  801afc:	8b 45 08             	mov    0x8(%ebp),%eax
  801aff:	8b 00                	mov    (%eax),%eax
  801b01:	8d 48 08             	lea    0x8(%eax),%ecx
  801b04:	8b 55 08             	mov    0x8(%ebp),%edx
  801b07:	89 0a                	mov    %ecx,(%edx)
  801b09:	8b 50 04             	mov    0x4(%eax),%edx
  801b0c:	8b 00                	mov    (%eax),%eax
  801b0e:	eb 30                	jmp    801b40 <getuint+0x4d>
    }
    else if (lflag) {
  801b10:	83 7d 0c 00          	cmpl   $0x0,0xc(%ebp)
  801b14:	74 16                	je     801b2c <getuint+0x39>
        return va_arg(*ap, unsigned long);
  801b16:	8b 45 08             	mov    0x8(%ebp),%eax
  801b19:	8b 00                	mov    (%eax),%eax
  801b1b:	8d 48 04             	lea    0x4(%eax),%ecx
  801b1e:	8b 55 08             	mov    0x8(%ebp),%edx
  801b21:	89 0a                	mov    %ecx,(%edx)
  801b23:	8b 00                	mov    (%eax),%eax
  801b25:	ba 00 00 00 00       	mov    $0x0,%edx
  801b2a:	eb 14                	jmp    801b40 <getuint+0x4d>
    }
    else {
        return va_arg(*ap, unsigned int);
  801b2c:	8b 45 08             	mov    0x8(%ebp),%eax
  801b2f:	8b 00                	mov    (%eax),%eax
  801b31:	8d 48 04             	lea    0x4(%eax),%ecx
  801b34:	8b 55 08             	mov    0x8(%ebp),%edx
  801b37:	89 0a                	mov    %ecx,(%edx)
  801b39:	8b 00                	mov    (%eax),%eax
  801b3b:	ba 00 00 00 00       	mov    $0x0,%edx
    }
}
  801b40:	5d                   	pop    %ebp
  801b41:	c3                   	ret    

00801b42 <getint>:
 * getint - same as getuint but signed, we can't use getuint because of sign extension
 * @ap:         a varargs list pointer
 * @lflag:      determines the size of the vararg that @ap points to
 * */
static long long getint(va_list *ap, int lflag)
{
  801b42:	55                   	push   %ebp
  801b43:	89 e5                	mov    %esp,%ebp
    if (lflag >= 2) {
  801b45:	83 7d 0c 01          	cmpl   $0x1,0xc(%ebp)
  801b49:	7e 14                	jle    801b5f <getint+0x1d>
        return va_arg(*ap, long long);
  801b4b:	8b 45 08             	mov    0x8(%ebp),%eax
  801b4e:	8b 00                	mov    (%eax),%eax
  801b50:	8d 48 08             	lea    0x8(%eax),%ecx
  801b53:	8b 55 08             	mov    0x8(%ebp),%edx
  801b56:	89 0a                	mov    %ecx,(%edx)
  801b58:	8b 50 04             	mov    0x4(%eax),%edx
  801b5b:	8b 00                	mov    (%eax),%eax
  801b5d:	eb 28                	jmp    801b87 <getint+0x45>
    }
    else if (lflag) {
  801b5f:	83 7d 0c 00          	cmpl   $0x0,0xc(%ebp)
  801b63:	74 12                	je     801b77 <getint+0x35>
        return va_arg(*ap, long);
  801b65:	8b 45 08             	mov    0x8(%ebp),%eax
  801b68:	8b 00                	mov    (%eax),%eax
  801b6a:	8d 48 04             	lea    0x4(%eax),%ecx
  801b6d:	8b 55 08             	mov    0x8(%ebp),%edx
  801b70:	89 0a                	mov    %ecx,(%edx)
  801b72:	8b 00                	mov    (%eax),%eax
  801b74:	99                   	cltd   
  801b75:	eb 10                	jmp    801b87 <getint+0x45>
    }
    else {
        return va_arg(*ap, int);
  801b77:	8b 45 08             	mov    0x8(%ebp),%eax
  801b7a:	8b 00                	mov    (%eax),%eax
  801b7c:	8d 48 04             	lea    0x4(%eax),%ecx
  801b7f:	8b 55 08             	mov    0x8(%ebp),%edx
  801b82:	89 0a                	mov    %ecx,(%edx)
  801b84:	8b 00                	mov    (%eax),%eax
  801b86:	99                   	cltd   
    }
}
  801b87:	5d                   	pop    %ebp
  801b88:	c3                   	ret    

00801b89 <printfmt>:
 * @fd:         file descriptor
 * @putdat:     used by @putch function
 * @fmt:        the format string to use
 * */
void printfmt(void (*putch)(int, void*, int), int fd, void *putdat, const char *fmt, ...)
{
  801b89:	55                   	push   %ebp
  801b8a:	89 e5                	mov    %esp,%ebp
  801b8c:	83 ec 18             	sub    $0x18,%esp
    va_list ap;

    va_start(ap, fmt);
  801b8f:	8d 45 18             	lea    0x18(%ebp),%eax
  801b92:	89 45 f4             	mov    %eax,-0xc(%ebp)
    vprintfmt(putch, fd, putdat, fmt, ap);
  801b95:	8b 45 f4             	mov    -0xc(%ebp),%eax
  801b98:	83 ec 0c             	sub    $0xc,%esp
  801b9b:	50                   	push   %eax
  801b9c:	ff 75 14             	pushl  0x14(%ebp)
  801b9f:	ff 75 10             	pushl  0x10(%ebp)
  801ba2:	ff 75 0c             	pushl  0xc(%ebp)
  801ba5:	ff 75 08             	pushl  0x8(%ebp)
  801ba8:	e8 06 00 00 00       	call   801bb3 <vprintfmt>
  801bad:	83 c4 20             	add    $0x20,%esp
    va_end(ap);
}
  801bb0:	90                   	nop
  801bb1:	c9                   	leave  
  801bb2:	c3                   	ret    

00801bb3 <vprintfmt>:
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want printfmt() instead.
 * */
void vprintfmt(void (*putch)(int, void*, int), int fd, void *putdat, const char *fmt, va_list ap)
{
  801bb3:	55                   	push   %ebp
  801bb4:	89 e5                	mov    %esp,%ebp
  801bb6:	56                   	push   %esi
  801bb7:	53                   	push   %ebx
  801bb8:	83 ec 20             	sub    $0x20,%esp
    register int ch, err;
    unsigned long long num;
    int base, width, precision, lflag, altflag;

    while (1) {
        while ((ch = *(unsigned char *)fmt ++) != '%') {
  801bbb:	eb 1a                	jmp    801bd7 <vprintfmt+0x24>
            if (ch == '\0') {
  801bbd:	85 db                	test   %ebx,%ebx
  801bbf:	0f 84 ac 03 00 00    	je     801f71 <vprintfmt+0x3be>
                return;
            }
            putch(ch, putdat, fd);
  801bc5:	83 ec 04             	sub    $0x4,%esp
  801bc8:	ff 75 0c             	pushl  0xc(%ebp)
  801bcb:	ff 75 10             	pushl  0x10(%ebp)
  801bce:	53                   	push   %ebx
  801bcf:	8b 45 08             	mov    0x8(%ebp),%eax
  801bd2:	ff d0                	call   *%eax
  801bd4:	83 c4 10             	add    $0x10,%esp
        while ((ch = *(unsigned char *)fmt ++) != '%') {
  801bd7:	8b 45 14             	mov    0x14(%ebp),%eax
  801bda:	8d 50 01             	lea    0x1(%eax),%edx
  801bdd:	89 55 14             	mov    %edx,0x14(%ebp)
  801be0:	8a 00                	mov    (%eax),%al
  801be2:	0f b6 d8             	movzbl %al,%ebx
  801be5:	83 fb 25             	cmp    $0x25,%ebx
  801be8:	75 d3                	jne    801bbd <vprintfmt+0xa>
        }

        // Process a %-escape sequence
        char padc = ' ';
  801bea:	c6 45 db 20          	movb   $0x20,-0x25(%ebp)
        width = precision = -1;
  801bee:	c7 45 e4 ff ff ff ff 	movl   $0xffffffff,-0x1c(%ebp)
  801bf5:	8b 45 e4             	mov    -0x1c(%ebp),%eax
  801bf8:	89 45 e8             	mov    %eax,-0x18(%ebp)
        lflag = altflag = 0;
  801bfb:	c7 45 dc 00 00 00 00 	movl   $0x0,-0x24(%ebp)
  801c02:	8b 45 dc             	mov    -0x24(%ebp),%eax
  801c05:	89 45 e0             	mov    %eax,-0x20(%ebp)

    reswitch:
        switch (ch = *(unsigned char *)fmt ++) {
  801c08:	8b 45 14             	mov    0x14(%ebp),%eax
  801c0b:	8d 50 01             	lea    0x1(%eax),%edx
  801c0e:	89 55 14             	mov    %edx,0x14(%ebp)
  801c11:	8a 00                	mov    (%eax),%al
  801c13:	0f b6 d8             	movzbl %al,%ebx
  801c16:	8d 43 dd             	lea    -0x23(%ebx),%eax
  801c19:	83 f8 55             	cmp    $0x55,%eax
  801c1c:	0f 87 24 03 00 00    	ja     801f46 <vprintfmt+0x393>
  801c22:	8b 04 85 7c 2c 80 00 	mov    0x802c7c(,%eax,4),%eax
  801c29:	ff e0                	jmp    *%eax

        // flag to pad on the right
        case '-':
            padc = '-';
  801c2b:	c6 45 db 2d          	movb   $0x2d,-0x25(%ebp)
            goto reswitch;
  801c2f:	eb d7                	jmp    801c08 <vprintfmt+0x55>

        // flag to pad with 0's instead of spaces
        case '0':
            padc = '0';
  801c31:	c6 45 db 30          	movb   $0x30,-0x25(%ebp)
            goto reswitch;
  801c35:	eb d1                	jmp    801c08 <vprintfmt+0x55>

        // width field
        case '1' ... '9':
            for (precision = 0; ; ++ fmt) {
  801c37:	c7 45 e4 00 00 00 00 	movl   $0x0,-0x1c(%ebp)
                precision = precision * 10 + ch - '0';
  801c3e:	8b 55 e4             	mov    -0x1c(%ebp),%edx
  801c41:	89 d0                	mov    %edx,%eax
  801c43:	c1 e0 02             	shl    $0x2,%eax
  801c46:	01 d0                	add    %edx,%eax
  801c48:	01 c0                	add    %eax,%eax
  801c4a:	01 d8                	add    %ebx,%eax
  801c4c:	83 e8 30             	sub    $0x30,%eax
  801c4f:	89 45 e4             	mov    %eax,-0x1c(%ebp)
                ch = *fmt;
  801c52:	8b 45 14             	mov    0x14(%ebp),%eax
  801c55:	8a 00                	mov    (%eax),%al
  801c57:	0f be d8             	movsbl %al,%ebx
                if (ch < '0' || ch > '9') {
  801c5a:	83 fb 2f             	cmp    $0x2f,%ebx
  801c5d:	7e 35                	jle    801c94 <vprintfmt+0xe1>
  801c5f:	83 fb 39             	cmp    $0x39,%ebx
  801c62:	7f 30                	jg     801c94 <vprintfmt+0xe1>
            for (precision = 0; ; ++ fmt) {
  801c64:	ff 45 14             	incl   0x14(%ebp)
                precision = precision * 10 + ch - '0';
  801c67:	eb d5                	jmp    801c3e <vprintfmt+0x8b>
                }
            }
            goto process_precision;

        case '*':
            precision = va_arg(ap, int);
  801c69:	8b 45 18             	mov    0x18(%ebp),%eax
  801c6c:	8d 50 04             	lea    0x4(%eax),%edx
  801c6f:	89 55 18             	mov    %edx,0x18(%ebp)
  801c72:	8b 00                	mov    (%eax),%eax
  801c74:	89 45 e4             	mov    %eax,-0x1c(%ebp)
            goto process_precision;
  801c77:	eb 1c                	jmp    801c95 <vprintfmt+0xe2>

        case '.':
            if (width < 0)
  801c79:	83 7d e8 00          	cmpl   $0x0,-0x18(%ebp)
  801c7d:	79 89                	jns    801c08 <vprintfmt+0x55>
                width = 0;
  801c7f:	c7 45 e8 00 00 00 00 	movl   $0x0,-0x18(%ebp)
            goto reswitch;
  801c86:	eb 80                	jmp    801c08 <vprintfmt+0x55>

        case '#':
            altflag = 1;
  801c88:	c7 45 dc 01 00 00 00 	movl   $0x1,-0x24(%ebp)
            goto reswitch;
  801c8f:	e9 74 ff ff ff       	jmp    801c08 <vprintfmt+0x55>
            goto process_precision;
  801c94:	90                   	nop

        process_precision:
            if (width < 0)
  801c95:	83 7d e8 00          	cmpl   $0x0,-0x18(%ebp)
  801c99:	0f 89 69 ff ff ff    	jns    801c08 <vprintfmt+0x55>
            {
                width = precision;
  801c9f:	8b 45 e4             	mov    -0x1c(%ebp),%eax
  801ca2:	89 45 e8             	mov    %eax,-0x18(%ebp)
                precision = -1;
  801ca5:	c7 45 e4 ff ff ff ff 	movl   $0xffffffff,-0x1c(%ebp)
            }
            goto reswitch;
  801cac:	e9 57 ff ff ff       	jmp    801c08 <vprintfmt+0x55>

        // long flag (doubled for long long)
        case 'l':
            lflag ++;
  801cb1:	ff 45 e0             	incl   -0x20(%ebp)
            goto reswitch;
  801cb4:	e9 4f ff ff ff       	jmp    801c08 <vprintfmt+0x55>

        // character
        case 'c':
            putch(va_arg(ap, int), putdat, fd);
  801cb9:	8b 45 18             	mov    0x18(%ebp),%eax
  801cbc:	8d 50 04             	lea    0x4(%eax),%edx
  801cbf:	89 55 18             	mov    %edx,0x18(%ebp)
  801cc2:	8b 00                	mov    (%eax),%eax
  801cc4:	83 ec 04             	sub    $0x4,%esp
  801cc7:	ff 75 0c             	pushl  0xc(%ebp)
  801cca:	ff 75 10             	pushl  0x10(%ebp)
  801ccd:	50                   	push   %eax
  801cce:	8b 45 08             	mov    0x8(%ebp),%eax
  801cd1:	ff d0                	call   *%eax
  801cd3:	83 c4 10             	add    $0x10,%esp
            break;
  801cd6:	e9 91 02 00 00       	jmp    801f6c <vprintfmt+0x3b9>

        // error message
        case 'e':
            err = va_arg(ap, int);
  801cdb:	8b 45 18             	mov    0x18(%ebp),%eax
  801cde:	8d 50 04             	lea    0x4(%eax),%edx
  801ce1:	89 55 18             	mov    %edx,0x18(%ebp)
  801ce4:	8b 18                	mov    (%eax),%ebx
            if (err < 0) {
  801ce6:	85 db                	test   %ebx,%ebx
  801ce8:	79 02                	jns    801cec <vprintfmt+0x139>
                err = -err;
  801cea:	f7 db                	neg    %ebx
            }
            if (err > MAXERROR || (p = error_string[err]) == NULL) {
  801cec:	83 fb 1d             	cmp    $0x1d,%ebx
  801cef:	7f 0b                	jg     801cfc <vprintfmt+0x149>
  801cf1:	8b 34 9d e0 2b 80 00 	mov    0x802be0(,%ebx,4),%esi
  801cf8:	85 f6                	test   %esi,%esi
  801cfa:	75 1f                	jne    801d1b <vprintfmt+0x168>
                printfmt(putch, fd, putdat, "error %d", err);
  801cfc:	83 ec 0c             	sub    $0xc,%esp
  801cff:	53                   	push   %ebx
  801d00:	68 69 2c 80 00       	push   $0x802c69
  801d05:	ff 75 10             	pushl  0x10(%ebp)
  801d08:	ff 75 0c             	pushl  0xc(%ebp)
  801d0b:	ff 75 08             	pushl  0x8(%ebp)
  801d0e:	e8 76 fe ff ff       	call   801b89 <printfmt>
  801d13:	83 c4 20             	add    $0x20,%esp
            }
            else {
                printfmt(putch, fd, putdat, "%s", p);
            }
            break;
  801d16:	e9 51 02 00 00       	jmp    801f6c <vprintfmt+0x3b9>
                printfmt(putch, fd, putdat, "%s", p);
  801d1b:	83 ec 0c             	sub    $0xc,%esp
  801d1e:	56                   	push   %esi
  801d1f:	68 72 2c 80 00       	push   $0x802c72
  801d24:	ff 75 10             	pushl  0x10(%ebp)
  801d27:	ff 75 0c             	pushl  0xc(%ebp)
  801d2a:	ff 75 08             	pushl  0x8(%ebp)
  801d2d:	e8 57 fe ff ff       	call   801b89 <printfmt>
  801d32:	83 c4 20             	add    $0x20,%esp
            break;
  801d35:	e9 32 02 00 00       	jmp    801f6c <vprintfmt+0x3b9>

        // string
        case 's':
            if ((p = va_arg(ap, char *)) == NULL) {
  801d3a:	8b 45 18             	mov    0x18(%ebp),%eax
  801d3d:	8d 50 04             	lea    0x4(%eax),%edx
  801d40:	89 55 18             	mov    %edx,0x18(%ebp)
  801d43:	8b 30                	mov    (%eax),%esi
  801d45:	85 f6                	test   %esi,%esi
  801d47:	75 05                	jne    801d4e <vprintfmt+0x19b>
                p = "(null)";
  801d49:	be 75 2c 80 00       	mov    $0x802c75,%esi
            }
            if (width > 0 && padc != '-') {
  801d4e:	83 7d e8 00          	cmpl   $0x0,-0x18(%ebp)
  801d52:	7e 7d                	jle    801dd1 <vprintfmt+0x21e>
  801d54:	80 7d db 2d          	cmpb   $0x2d,-0x25(%ebp)
  801d58:	74 77                	je     801dd1 <vprintfmt+0x21e>
                for (width -= strnlen(p, precision); width > 0; width --) {
  801d5a:	8b 45 e4             	mov    -0x1c(%ebp),%eax
  801d5d:	83 ec 08             	sub    $0x8,%esp
  801d60:	50                   	push   %eax
  801d61:	56                   	push   %esi
  801d62:	e8 d6 03 00 00       	call   80213d <strnlen>
  801d67:	83 c4 10             	add    $0x10,%esp
  801d6a:	8b 55 e8             	mov    -0x18(%ebp),%edx
  801d6d:	29 c2                	sub    %eax,%edx
  801d6f:	89 d0                	mov    %edx,%eax
  801d71:	89 45 e8             	mov    %eax,-0x18(%ebp)
  801d74:	eb 19                	jmp    801d8f <vprintfmt+0x1dc>
                    putch(padc, putdat, fd);
  801d76:	0f be 45 db          	movsbl -0x25(%ebp),%eax
  801d7a:	83 ec 04             	sub    $0x4,%esp
  801d7d:	ff 75 0c             	pushl  0xc(%ebp)
  801d80:	ff 75 10             	pushl  0x10(%ebp)
  801d83:	50                   	push   %eax
  801d84:	8b 45 08             	mov    0x8(%ebp),%eax
  801d87:	ff d0                	call   *%eax
  801d89:	83 c4 10             	add    $0x10,%esp
                for (width -= strnlen(p, precision); width > 0; width --) {
  801d8c:	ff 4d e8             	decl   -0x18(%ebp)
  801d8f:	83 7d e8 00          	cmpl   $0x0,-0x18(%ebp)
  801d93:	7f e1                	jg     801d76 <vprintfmt+0x1c3>
                }
            }
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
  801d95:	eb 3a                	jmp    801dd1 <vprintfmt+0x21e>
                if (altflag && (ch < ' ' || ch > '~')) {
  801d97:	83 7d dc 00          	cmpl   $0x0,-0x24(%ebp)
  801d9b:	74 1f                	je     801dbc <vprintfmt+0x209>
  801d9d:	83 fb 1f             	cmp    $0x1f,%ebx
  801da0:	7e 05                	jle    801da7 <vprintfmt+0x1f4>
  801da2:	83 fb 7e             	cmp    $0x7e,%ebx
  801da5:	7e 15                	jle    801dbc <vprintfmt+0x209>
                    putch('?', putdat, fd);
  801da7:	83 ec 04             	sub    $0x4,%esp
  801daa:	ff 75 0c             	pushl  0xc(%ebp)
  801dad:	ff 75 10             	pushl  0x10(%ebp)
  801db0:	6a 3f                	push   $0x3f
  801db2:	8b 45 08             	mov    0x8(%ebp),%eax
  801db5:	ff d0                	call   *%eax
  801db7:	83 c4 10             	add    $0x10,%esp
  801dba:	eb 12                	jmp    801dce <vprintfmt+0x21b>
                }
                else {
                    putch(ch, putdat, fd);
  801dbc:	83 ec 04             	sub    $0x4,%esp
  801dbf:	ff 75 0c             	pushl  0xc(%ebp)
  801dc2:	ff 75 10             	pushl  0x10(%ebp)
  801dc5:	53                   	push   %ebx
  801dc6:	8b 45 08             	mov    0x8(%ebp),%eax
  801dc9:	ff d0                	call   *%eax
  801dcb:	83 c4 10             	add    $0x10,%esp
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
  801dce:	ff 4d e8             	decl   -0x18(%ebp)
  801dd1:	89 f0                	mov    %esi,%eax
  801dd3:	8d 70 01             	lea    0x1(%eax),%esi
  801dd6:	8a 00                	mov    (%eax),%al
  801dd8:	0f be d8             	movsbl %al,%ebx
  801ddb:	85 db                	test   %ebx,%ebx
  801ddd:	74 27                	je     801e06 <vprintfmt+0x253>
  801ddf:	83 7d e4 00          	cmpl   $0x0,-0x1c(%ebp)
  801de3:	78 b2                	js     801d97 <vprintfmt+0x1e4>
  801de5:	ff 4d e4             	decl   -0x1c(%ebp)
  801de8:	83 7d e4 00          	cmpl   $0x0,-0x1c(%ebp)
  801dec:	79 a9                	jns    801d97 <vprintfmt+0x1e4>
                }
            }
            for (; width > 0; width --) {
  801dee:	eb 16                	jmp    801e06 <vprintfmt+0x253>
                putch(' ', putdat, fd);
  801df0:	83 ec 04             	sub    $0x4,%esp
  801df3:	ff 75 0c             	pushl  0xc(%ebp)
  801df6:	ff 75 10             	pushl  0x10(%ebp)
  801df9:	6a 20                	push   $0x20
  801dfb:	8b 45 08             	mov    0x8(%ebp),%eax
  801dfe:	ff d0                	call   *%eax
  801e00:	83 c4 10             	add    $0x10,%esp
            for (; width > 0; width --) {
  801e03:	ff 4d e8             	decl   -0x18(%ebp)
  801e06:	83 7d e8 00          	cmpl   $0x0,-0x18(%ebp)
  801e0a:	7f e4                	jg     801df0 <vprintfmt+0x23d>
            }
            break;
  801e0c:	e9 5b 01 00 00       	jmp    801f6c <vprintfmt+0x3b9>

        // (signed) decimal
        case 'd':
            num = getint(&ap, lflag);
  801e11:	83 ec 08             	sub    $0x8,%esp
  801e14:	ff 75 e0             	pushl  -0x20(%ebp)
  801e17:	8d 45 18             	lea    0x18(%ebp),%eax
  801e1a:	50                   	push   %eax
  801e1b:	e8 22 fd ff ff       	call   801b42 <getint>
  801e20:	83 c4 10             	add    $0x10,%esp
  801e23:	89 45 f0             	mov    %eax,-0x10(%ebp)
  801e26:	89 55 f4             	mov    %edx,-0xc(%ebp)
            if ((long long)num < 0) {
  801e29:	8b 45 f0             	mov    -0x10(%ebp),%eax
  801e2c:	8b 55 f4             	mov    -0xc(%ebp),%edx
  801e2f:	85 d2                	test   %edx,%edx
  801e31:	79 26                	jns    801e59 <vprintfmt+0x2a6>
                putch('-', putdat, fd);
  801e33:	83 ec 04             	sub    $0x4,%esp
  801e36:	ff 75 0c             	pushl  0xc(%ebp)
  801e39:	ff 75 10             	pushl  0x10(%ebp)
  801e3c:	6a 2d                	push   $0x2d
  801e3e:	8b 45 08             	mov    0x8(%ebp),%eax
  801e41:	ff d0                	call   *%eax
  801e43:	83 c4 10             	add    $0x10,%esp
                num = -(long long)num;
  801e46:	8b 45 f0             	mov    -0x10(%ebp),%eax
  801e49:	8b 55 f4             	mov    -0xc(%ebp),%edx
  801e4c:	f7 d8                	neg    %eax
  801e4e:	83 d2 00             	adc    $0x0,%edx
  801e51:	f7 da                	neg    %edx
  801e53:	89 45 f0             	mov    %eax,-0x10(%ebp)
  801e56:	89 55 f4             	mov    %edx,-0xc(%ebp)
            }
            base = 10;
  801e59:	c7 45 ec 0a 00 00 00 	movl   $0xa,-0x14(%ebp)
            goto number;
  801e60:	e9 a8 00 00 00       	jmp    801f0d <vprintfmt+0x35a>

        // unsigned decimal
        case 'u':
            num = getuint(&ap, lflag);
  801e65:	83 ec 08             	sub    $0x8,%esp
  801e68:	ff 75 e0             	pushl  -0x20(%ebp)
  801e6b:	8d 45 18             	lea    0x18(%ebp),%eax
  801e6e:	50                   	push   %eax
  801e6f:	e8 7f fc ff ff       	call   801af3 <getuint>
  801e74:	83 c4 10             	add    $0x10,%esp
  801e77:	89 45 f0             	mov    %eax,-0x10(%ebp)
  801e7a:	89 55 f4             	mov    %edx,-0xc(%ebp)
            base = 10;
  801e7d:	c7 45 ec 0a 00 00 00 	movl   $0xa,-0x14(%ebp)
            goto number;
  801e84:	e9 84 00 00 00       	jmp    801f0d <vprintfmt+0x35a>

        // (unsigned) octal
        case 'o':
            num = getuint(&ap, lflag);
  801e89:	83 ec 08             	sub    $0x8,%esp
  801e8c:	ff 75 e0             	pushl  -0x20(%ebp)
  801e8f:	8d 45 18             	lea    0x18(%ebp),%eax
  801e92:	50                   	push   %eax
  801e93:	e8 5b fc ff ff       	call   801af3 <getuint>
  801e98:	83 c4 10             	add    $0x10,%esp
  801e9b:	89 45 f0             	mov    %eax,-0x10(%ebp)
  801e9e:	89 55 f4             	mov    %edx,-0xc(%ebp)
            base = 8;
  801ea1:	c7 45 ec 08 00 00 00 	movl   $0x8,-0x14(%ebp)
            goto number;
  801ea8:	eb 63                	jmp    801f0d <vprintfmt+0x35a>

        // pointer
        case 'p':
            putch('0', putdat, fd);
  801eaa:	83 ec 04             	sub    $0x4,%esp
  801ead:	ff 75 0c             	pushl  0xc(%ebp)
  801eb0:	ff 75 10             	pushl  0x10(%ebp)
  801eb3:	6a 30                	push   $0x30
  801eb5:	8b 45 08             	mov    0x8(%ebp),%eax
  801eb8:	ff d0                	call   *%eax
  801eba:	83 c4 10             	add    $0x10,%esp
            putch('x', putdat, fd);
  801ebd:	83 ec 04             	sub    $0x4,%esp
  801ec0:	ff 75 0c             	pushl  0xc(%ebp)
  801ec3:	ff 75 10             	pushl  0x10(%ebp)
  801ec6:	6a 78                	push   $0x78
  801ec8:	8b 45 08             	mov    0x8(%ebp),%eax
  801ecb:	ff d0                	call   *%eax
  801ecd:	83 c4 10             	add    $0x10,%esp
            num = (unsigned long long)(uintptr_t)va_arg(ap, void *);
  801ed0:	8b 45 18             	mov    0x18(%ebp),%eax
  801ed3:	8d 50 04             	lea    0x4(%eax),%edx
  801ed6:	89 55 18             	mov    %edx,0x18(%ebp)
  801ed9:	8b 00                	mov    (%eax),%eax
  801edb:	89 45 f0             	mov    %eax,-0x10(%ebp)
  801ede:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
            base = 16;
  801ee5:	c7 45 ec 10 00 00 00 	movl   $0x10,-0x14(%ebp)
            goto number;
  801eec:	eb 1f                	jmp    801f0d <vprintfmt+0x35a>

        // (unsigned) hexadecimal
        case 'x':
            num = getuint(&ap, lflag);
  801eee:	83 ec 08             	sub    $0x8,%esp
  801ef1:	ff 75 e0             	pushl  -0x20(%ebp)
  801ef4:	8d 45 18             	lea    0x18(%ebp),%eax
  801ef7:	50                   	push   %eax
  801ef8:	e8 f6 fb ff ff       	call   801af3 <getuint>
  801efd:	83 c4 10             	add    $0x10,%esp
  801f00:	89 45 f0             	mov    %eax,-0x10(%ebp)
  801f03:	89 55 f4             	mov    %edx,-0xc(%ebp)
            base = 16;
  801f06:	c7 45 ec 10 00 00 00 	movl   $0x10,-0x14(%ebp)
        number:
            printnum(putch, fd, putdat, num, base, width, padc);
  801f0d:	0f be 55 db          	movsbl -0x25(%ebp),%edx
  801f11:	8b 45 ec             	mov    -0x14(%ebp),%eax
  801f14:	52                   	push   %edx
  801f15:	ff 75 e8             	pushl  -0x18(%ebp)
  801f18:	50                   	push   %eax
  801f19:	ff 75 f4             	pushl  -0xc(%ebp)
  801f1c:	ff 75 f0             	pushl  -0x10(%ebp)
  801f1f:	ff 75 10             	pushl  0x10(%ebp)
  801f22:	ff 75 0c             	pushl  0xc(%ebp)
  801f25:	ff 75 08             	pushl  0x8(%ebp)
  801f28:	e8 d8 fa ff ff       	call   801a05 <printnum>
  801f2d:	83 c4 20             	add    $0x20,%esp
            break;
  801f30:	eb 3a                	jmp    801f6c <vprintfmt+0x3b9>

        // escaped '%' character
        case '%':
            putch(ch, putdat, fd);
  801f32:	83 ec 04             	sub    $0x4,%esp
  801f35:	ff 75 0c             	pushl  0xc(%ebp)
  801f38:	ff 75 10             	pushl  0x10(%ebp)
  801f3b:	53                   	push   %ebx
  801f3c:	8b 45 08             	mov    0x8(%ebp),%eax
  801f3f:	ff d0                	call   *%eax
  801f41:	83 c4 10             	add    $0x10,%esp
            break;
  801f44:	eb 26                	jmp    801f6c <vprintfmt+0x3b9>

        // unrecognized escape sequence - just print it literally
        default:
            putch('%', putdat, fd);
  801f46:	83 ec 04             	sub    $0x4,%esp
  801f49:	ff 75 0c             	pushl  0xc(%ebp)
  801f4c:	ff 75 10             	pushl  0x10(%ebp)
  801f4f:	6a 25                	push   $0x25
  801f51:	8b 45 08             	mov    0x8(%ebp),%eax
  801f54:	ff d0                	call   *%eax
  801f56:	83 c4 10             	add    $0x10,%esp
            for (fmt --; fmt[-1] != '%'; fmt --)
  801f59:	ff 4d 14             	decl   0x14(%ebp)
  801f5c:	eb 03                	jmp    801f61 <vprintfmt+0x3ae>
  801f5e:	ff 4d 14             	decl   0x14(%ebp)
  801f61:	8b 45 14             	mov    0x14(%ebp),%eax
  801f64:	48                   	dec    %eax
  801f65:	8a 00                	mov    (%eax),%al
  801f67:	3c 25                	cmp    $0x25,%al
  801f69:	75 f3                	jne    801f5e <vprintfmt+0x3ab>
                /* do nothing */;
            break;
  801f6b:	90                   	nop
    while (1) {
  801f6c:	e9 4a fc ff ff       	jmp    801bbb <vprintfmt+0x8>
                return;
  801f71:	90                   	nop
        }
    }
}
  801f72:	8d 65 f8             	lea    -0x8(%ebp),%esp
  801f75:	5b                   	pop    %ebx
  801f76:	5e                   	pop    %esi
  801f77:	5d                   	pop    %ebp
  801f78:	c3                   	ret    

00801f79 <sprintputch>:
 * sprintputch - 'print' a single character in a buffer
 * @ch:         the character will be printed
 * @b:          the buffer to place the character @ch
 * */
static void sprintputch(int ch, struct sprintbuf *b)
{
  801f79:	55                   	push   %ebp
  801f7a:	89 e5                	mov    %esp,%ebp
    b->cnt ++;
  801f7c:	8b 45 0c             	mov    0xc(%ebp),%eax
  801f7f:	8b 40 08             	mov    0x8(%eax),%eax
  801f82:	8d 50 01             	lea    0x1(%eax),%edx
  801f85:	8b 45 0c             	mov    0xc(%ebp),%eax
  801f88:	89 50 08             	mov    %edx,0x8(%eax)
    if (b->buf < b->ebuf) {
  801f8b:	8b 45 0c             	mov    0xc(%ebp),%eax
  801f8e:	8b 10                	mov    (%eax),%edx
  801f90:	8b 45 0c             	mov    0xc(%ebp),%eax
  801f93:	8b 40 04             	mov    0x4(%eax),%eax
  801f96:	39 c2                	cmp    %eax,%edx
  801f98:	73 12                	jae    801fac <sprintputch+0x33>
        *b->buf ++ = ch;
  801f9a:	8b 45 0c             	mov    0xc(%ebp),%eax
  801f9d:	8b 00                	mov    (%eax),%eax
  801f9f:	8d 48 01             	lea    0x1(%eax),%ecx
  801fa2:	8b 55 0c             	mov    0xc(%ebp),%edx
  801fa5:	89 0a                	mov    %ecx,(%edx)
  801fa7:	8b 55 08             	mov    0x8(%ebp),%edx
  801faa:	88 10                	mov    %dl,(%eax)
    }
}
  801fac:	90                   	nop
  801fad:	5d                   	pop    %ebp
  801fae:	c3                   	ret    

00801faf <snprintf>:
 * @str:        the buffer to place the result into
 * @size:       the size of buffer, including the trailing null space
 * @fmt:        the format string to use
 * */
int snprintf(char *str, size_t size, const char *fmt, ...)
{
  801faf:	55                   	push   %ebp
  801fb0:	89 e5                	mov    %esp,%ebp
  801fb2:	83 ec 18             	sub    $0x18,%esp
    va_list ap;
    int cnt;
    va_start(ap, fmt);
  801fb5:	8d 45 14             	lea    0x14(%ebp),%eax
  801fb8:	89 45 f0             	mov    %eax,-0x10(%ebp)
    cnt = vsnprintf(str, size, fmt, ap);
  801fbb:	8b 45 f0             	mov    -0x10(%ebp),%eax
  801fbe:	50                   	push   %eax
  801fbf:	ff 75 10             	pushl  0x10(%ebp)
  801fc2:	ff 75 0c             	pushl  0xc(%ebp)
  801fc5:	ff 75 08             	pushl  0x8(%ebp)
  801fc8:	e8 0b 00 00 00       	call   801fd8 <vsnprintf>
  801fcd:	83 c4 10             	add    $0x10,%esp
  801fd0:	89 45 f4             	mov    %eax,-0xc(%ebp)
    va_end(ap);
    return cnt;
  801fd3:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
  801fd6:	c9                   	leave  
  801fd7:	c3                   	ret    

00801fd8 <vsnprintf>:
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want snprintf() instead.
 * */
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
  801fd8:	55                   	push   %ebp
  801fd9:	89 e5                	mov    %esp,%ebp
  801fdb:	83 ec 18             	sub    $0x18,%esp
    struct sprintbuf b = {str, str + size - 1, 0};
  801fde:	8b 45 08             	mov    0x8(%ebp),%eax
  801fe1:	89 45 ec             	mov    %eax,-0x14(%ebp)
  801fe4:	8b 45 0c             	mov    0xc(%ebp),%eax
  801fe7:	8d 50 ff             	lea    -0x1(%eax),%edx
  801fea:	8b 45 08             	mov    0x8(%ebp),%eax
  801fed:	01 d0                	add    %edx,%eax
  801fef:	89 45 f0             	mov    %eax,-0x10(%ebp)
  801ff2:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
    if (str == NULL || b.buf > b.ebuf) {
  801ff9:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
  801ffd:	74 0a                	je     802009 <vsnprintf+0x31>
  801fff:	8b 55 ec             	mov    -0x14(%ebp),%edx
  802002:	8b 45 f0             	mov    -0x10(%ebp),%eax
  802005:	39 c2                	cmp    %eax,%edx
  802007:	76 07                	jbe    802010 <vsnprintf+0x38>
        return -E_INVAL;
  802009:	b8 fd ff ff ff       	mov    $0xfffffffd,%eax
  80200e:	eb 28                	jmp    802038 <vsnprintf+0x60>
    }
    // print the string to the buffer
    vprintfmt((void*)sprintputch, NO_FD, &b, fmt, ap);
  802010:	83 ec 0c             	sub    $0xc,%esp
  802013:	ff 75 14             	pushl  0x14(%ebp)
  802016:	ff 75 10             	pushl  0x10(%ebp)
  802019:	8d 45 ec             	lea    -0x14(%ebp),%eax
  80201c:	50                   	push   %eax
  80201d:	68 d9 6a ff ff       	push   $0xffff6ad9
  802022:	68 79 1f 80 00       	push   $0x801f79
  802027:	e8 87 fb ff ff       	call   801bb3 <vprintfmt>
  80202c:	83 c4 20             	add    $0x20,%esp
    // null terminate the buffer
    *b.buf = '\0';
  80202f:	8b 45 ec             	mov    -0x14(%ebp),%eax
  802032:	c6 00 00             	movb   $0x0,(%eax)
    return b.cnt;
  802035:	8b 45 f4             	mov    -0xc(%ebp),%eax
}
  802038:	c9                   	leave  
  802039:	c3                   	ret    

0080203a <rand>:
 * rand - returns a pseudo-random integer
 *
 * The rand() function return a value in the range [0, RAND_MAX].
 * */
int rand(void)
{
  80203a:	55                   	push   %ebp
  80203b:	89 e5                	mov    %esp,%ebp
  80203d:	57                   	push   %edi
  80203e:	56                   	push   %esi
  80203f:	53                   	push   %ebx
  802040:	83 ec 24             	sub    $0x24,%esp
    next = (next * 0x5DEECE66DLL + 0xBLL) & ((1LL << 48) - 1);
  802043:	a1 a8 30 80 00       	mov    0x8030a8,%eax
  802048:	8b 15 ac 30 80 00    	mov    0x8030ac,%edx
  80204e:	69 fa 6d e6 ec de    	imul   $0xdeece66d,%edx,%edi
  802054:	6b f0 05             	imul   $0x5,%eax,%esi
  802057:	01 fe                	add    %edi,%esi
  802059:	bf 6d e6 ec de       	mov    $0xdeece66d,%edi
  80205e:	f7 e7                	mul    %edi
  802060:	01 d6                	add    %edx,%esi
  802062:	89 f2                	mov    %esi,%edx
  802064:	83 c0 0b             	add    $0xb,%eax
  802067:	83 d2 00             	adc    $0x0,%edx
  80206a:	89 c7                	mov    %eax,%edi
  80206c:	83 e7 ff             	and    $0xffffffff,%edi
  80206f:	89 f9                	mov    %edi,%ecx
  802071:	0f b7 da             	movzwl %dx,%ebx
  802074:	89 0d a8 30 80 00    	mov    %ecx,0x8030a8
  80207a:	89 1d ac 30 80 00    	mov    %ebx,0x8030ac
    unsigned long long result = (next >> 12);
  802080:	8b 1d a8 30 80 00    	mov    0x8030a8,%ebx
  802086:	8b 35 ac 30 80 00    	mov    0x8030ac,%esi
  80208c:	89 d8                	mov    %ebx,%eax
  80208e:	89 f2                	mov    %esi,%edx
  802090:	0f ac d0 0c          	shrd   $0xc,%edx,%eax
  802094:	c1 ea 0c             	shr    $0xc,%edx
  802097:	89 45 e0             	mov    %eax,-0x20(%ebp)
  80209a:	89 55 e4             	mov    %edx,-0x1c(%ebp)
    return (int)do_div(result, RAND_MAX + 1);
  80209d:	c7 45 dc 00 00 00 80 	movl   $0x80000000,-0x24(%ebp)
  8020a4:	8b 45 e0             	mov    -0x20(%ebp),%eax
  8020a7:	8b 55 e4             	mov    -0x1c(%ebp),%edx
  8020aa:	89 45 d8             	mov    %eax,-0x28(%ebp)
  8020ad:	89 55 e8             	mov    %edx,-0x18(%ebp)
  8020b0:	8b 45 e8             	mov    -0x18(%ebp),%eax
  8020b3:	89 45 ec             	mov    %eax,-0x14(%ebp)
  8020b6:	83 7d e8 00          	cmpl   $0x0,-0x18(%ebp)
  8020ba:	74 1c                	je     8020d8 <rand+0x9e>
  8020bc:	8b 45 e8             	mov    -0x18(%ebp),%eax
  8020bf:	ba 00 00 00 00       	mov    $0x0,%edx
  8020c4:	f7 75 dc             	divl   -0x24(%ebp)
  8020c7:	89 55 ec             	mov    %edx,-0x14(%ebp)
  8020ca:	8b 45 e8             	mov    -0x18(%ebp),%eax
  8020cd:	ba 00 00 00 00       	mov    $0x0,%edx
  8020d2:	f7 75 dc             	divl   -0x24(%ebp)
  8020d5:	89 45 e8             	mov    %eax,-0x18(%ebp)
  8020d8:	8b 45 d8             	mov    -0x28(%ebp),%eax
  8020db:	8b 55 ec             	mov    -0x14(%ebp),%edx
  8020de:	f7 75 dc             	divl   -0x24(%ebp)
  8020e1:	89 45 d8             	mov    %eax,-0x28(%ebp)
  8020e4:	89 55 d4             	mov    %edx,-0x2c(%ebp)
  8020e7:	8b 45 d8             	mov    -0x28(%ebp),%eax
  8020ea:	8b 55 e8             	mov    -0x18(%ebp),%edx
  8020ed:	89 45 e0             	mov    %eax,-0x20(%ebp)
  8020f0:	89 55 e4             	mov    %edx,-0x1c(%ebp)
  8020f3:	8b 45 d4             	mov    -0x2c(%ebp),%eax
}
  8020f6:	83 c4 24             	add    $0x24,%esp
  8020f9:	5b                   	pop    %ebx
  8020fa:	5e                   	pop    %esi
  8020fb:	5f                   	pop    %edi
  8020fc:	5d                   	pop    %ebp
  8020fd:	c3                   	ret    

008020fe <srand>:
/* *
 * srand - seed the random number generator with the given number
 * @seed:   the required seed number
 * */
void srand(unsigned int seed)
{
  8020fe:	55                   	push   %ebp
  8020ff:	89 e5                	mov    %esp,%ebp
    next = seed;
  802101:	8b 45 08             	mov    0x8(%ebp),%eax
  802104:	ba 00 00 00 00       	mov    $0x0,%edx
  802109:	a3 a8 30 80 00       	mov    %eax,0x8030a8
  80210e:	89 15 ac 30 80 00    	mov    %edx,0x8030ac
}
  802114:	90                   	nop
  802115:	5d                   	pop    %ebp
  802116:	c3                   	ret    

00802117 <strlen>:
 * @s:      the input string
 *
 * The strlen() function returns the length of string @s.
 * */
size_t strlen(const char *s)
{
  802117:	55                   	push   %ebp
  802118:	89 e5                	mov    %esp,%ebp
  80211a:	83 ec 10             	sub    $0x10,%esp
    size_t cnt = 0;
  80211d:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%ebp)
    while (*s ++ != '\0')
  802124:	eb 03                	jmp    802129 <strlen+0x12>
    {
        cnt ++;
  802126:	ff 45 fc             	incl   -0x4(%ebp)
    while (*s ++ != '\0')
  802129:	8b 45 08             	mov    0x8(%ebp),%eax
  80212c:	8d 50 01             	lea    0x1(%eax),%edx
  80212f:	89 55 08             	mov    %edx,0x8(%ebp)
  802132:	8a 00                	mov    (%eax),%al
  802134:	84 c0                	test   %al,%al
  802136:	75 ee                	jne    802126 <strlen+0xf>
    }
    return cnt;
  802138:	8b 45 fc             	mov    -0x4(%ebp),%eax
}
  80213b:	c9                   	leave  
  80213c:	c3                   	ret    

0080213d <strnlen>:
 * The return value is strlen(s), if that is less than @len, or
 * @len if there is no '\0' character among the first @len characters
 * pointed by @s.
 * */
size_t strnlen(const char *s, size_t len)
{
  80213d:	55                   	push   %ebp
  80213e:	89 e5                	mov    %esp,%ebp
  802140:	83 ec 10             	sub    $0x10,%esp
    size_t cnt = 0;
  802143:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%ebp)
    while (cnt < len && *s ++ != '\0')
  80214a:	eb 03                	jmp    80214f <strnlen+0x12>
    {
        cnt ++;
  80214c:	ff 45 fc             	incl   -0x4(%ebp)
    while (cnt < len && *s ++ != '\0')
  80214f:	8b 45 fc             	mov    -0x4(%ebp),%eax
  802152:	3b 45 0c             	cmp    0xc(%ebp),%eax
  802155:	73 0f                	jae    802166 <strnlen+0x29>
  802157:	8b 45 08             	mov    0x8(%ebp),%eax
  80215a:	8d 50 01             	lea    0x1(%eax),%edx
  80215d:	89 55 08             	mov    %edx,0x8(%ebp)
  802160:	8a 00                	mov    (%eax),%al
  802162:	84 c0                	test   %al,%al
  802164:	75 e6                	jne    80214c <strnlen+0xf>
    }
    return cnt;
  802166:	8b 45 fc             	mov    -0x4(%ebp),%eax
}
  802169:	c9                   	leave  
  80216a:	c3                   	ret    

0080216b <strcat>:
 * @dst:    pointer to the @dst array, which should be large enough to contain the concatenated
 *          resulting string.
 * @src:    string to be appended, this should not overlap @dst
 * */
char *strcat(char *dst, const char *src)
{
  80216b:	55                   	push   %ebp
  80216c:	89 e5                	mov    %esp,%ebp
  80216e:	83 ec 08             	sub    $0x8,%esp
    return strcpy(dst + strlen(dst), src);
  802171:	ff 75 08             	pushl  0x8(%ebp)
  802174:	e8 9e ff ff ff       	call   802117 <strlen>
  802179:	83 c4 04             	add    $0x4,%esp
  80217c:	89 c2                	mov    %eax,%edx
  80217e:	8b 45 08             	mov    0x8(%ebp),%eax
  802181:	01 d0                	add    %edx,%eax
  802183:	83 ec 08             	sub    $0x8,%esp
  802186:	ff 75 0c             	pushl  0xc(%ebp)
  802189:	50                   	push   %eax
  80218a:	e8 05 00 00 00       	call   802194 <strcpy>
  80218f:	83 c4 10             	add    $0x10,%esp
}
  802192:	c9                   	leave  
  802193:	c3                   	ret    

00802194 <strcpy>:
 * To avoid overflows, the size of array pointed by @dst should be long enough to
 * contain the same string as @src (including the terminating null character), and
 * should not overlap in memory with @src.
 * */
char *strcpy(char *dst, const char *src)
{
  802194:	55                   	push   %ebp
  802195:	89 e5                	mov    %esp,%ebp
  802197:	57                   	push   %edi
  802198:	56                   	push   %esi
  802199:	83 ec 20             	sub    $0x20,%esp
  80219c:	8b 45 08             	mov    0x8(%ebp),%eax
  80219f:	89 45 f4             	mov    %eax,-0xc(%ebp)
  8021a2:	8b 45 0c             	mov    0xc(%ebp),%eax
  8021a5:	89 45 f0             	mov    %eax,-0x10(%ebp)
#define __HAVE_ARCH_STRCPY
static inline char *__strcpy(char *dst, const char *src)
{
#if ASM_NO_64
    int d0, d1, d2;
    asm volatile (
  8021a8:	8b 55 f0             	mov    -0x10(%ebp),%edx
  8021ab:	8b 45 f4             	mov    -0xc(%ebp),%eax
  8021ae:	89 d1                	mov    %edx,%ecx
  8021b0:	89 c2                	mov    %eax,%edx
  8021b2:	89 ce                	mov    %ecx,%esi
  8021b4:	89 d7                	mov    %edx,%edi
  8021b6:	ac                   	lods   %ds:(%esi),%al
  8021b7:	aa                   	stos   %al,%es:(%edi)
  8021b8:	84 c0                	test   %al,%al
  8021ba:	75 fa                	jne    8021b6 <strcpy+0x22>
  8021bc:	89 fa                	mov    %edi,%edx
  8021be:	89 f1                	mov    %esi,%ecx
  8021c0:	89 4d ec             	mov    %ecx,-0x14(%ebp)
  8021c3:	89 55 e8             	mov    %edx,-0x18(%ebp)
  8021c6:	89 45 e4             	mov    %eax,-0x1c(%ebp)
        "testb %%al, %%al;"
        "jne 1b;"
        : "=&S" (d0), "=&D" (d1), "=&a" (d2)
        : "0" (src), "1" (dst) : "memory");
#endif
    return dst;
  8021c9:	8b 45 f4             	mov    -0xc(%ebp),%eax
#ifdef __HAVE_ARCH_STRCPY
    return __strcpy(dst, src);
  8021cc:	90                   	nop
    char *p = dst;
    while ((*p ++ = *src ++) != '\0')
        /* nothing */;
    return dst;
#endif /* __HAVE_ARCH_STRCPY */
}
  8021cd:	83 c4 20             	add    $0x20,%esp
  8021d0:	5e                   	pop    %esi
  8021d1:	5f                   	pop    %edi
  8021d2:	5d                   	pop    %ebp
  8021d3:	c3                   	ret    

008021d4 <strncpy>:
 * @len:    maximum number of characters to be copied from @src
 *
 * The return value is @dst
 * */
char *strncpy(char *dst, const char *src, size_t len)
{
  8021d4:	55                   	push   %ebp
  8021d5:	89 e5                	mov    %esp,%ebp
  8021d7:	83 ec 10             	sub    $0x10,%esp
    char *p = dst;
  8021da:	8b 45 08             	mov    0x8(%ebp),%eax
  8021dd:	89 45 fc             	mov    %eax,-0x4(%ebp)
    while (len > 0)
  8021e0:	eb 1c                	jmp    8021fe <strncpy+0x2a>
    {
        if ((*p = *src) != '\0')
  8021e2:	8b 45 0c             	mov    0xc(%ebp),%eax
  8021e5:	8a 10                	mov    (%eax),%dl
  8021e7:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8021ea:	88 10                	mov    %dl,(%eax)
  8021ec:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8021ef:	8a 00                	mov    (%eax),%al
  8021f1:	84 c0                	test   %al,%al
  8021f3:	74 03                	je     8021f8 <strncpy+0x24>
        {
            src ++;
  8021f5:	ff 45 0c             	incl   0xc(%ebp)
        }
        p ++;
  8021f8:	ff 45 fc             	incl   -0x4(%ebp)
        len --;
  8021fb:	ff 4d 10             	decl   0x10(%ebp)
    while (len > 0)
  8021fe:	83 7d 10 00          	cmpl   $0x0,0x10(%ebp)
  802202:	75 de                	jne    8021e2 <strncpy+0xe>
    }
    return dst;
  802204:	8b 45 08             	mov    0x8(%ebp),%eax
}
  802207:	c9                   	leave  
  802208:	c3                   	ret    

00802209 <strcmp>:
 * - A value greater than zero indicates that the first character that does
 *   not match has a greater value in @s1 than in @s2;
 * - And a value less than zero indicates the opposite.
 * */
int strcmp(const char *s1, const char *s2)
{
  802209:	55                   	push   %ebp
  80220a:	89 e5                	mov    %esp,%ebp
  80220c:	57                   	push   %edi
  80220d:	56                   	push   %esi
  80220e:	83 ec 20             	sub    $0x20,%esp
  802211:	8b 45 08             	mov    0x8(%ebp),%eax
  802214:	89 45 f4             	mov    %eax,-0xc(%ebp)
  802217:	8b 45 0c             	mov    0xc(%ebp),%eax
  80221a:	89 45 f0             	mov    %eax,-0x10(%ebp)
    int ret = 0;
  80221d:	c7 45 ec 00 00 00 00 	movl   $0x0,-0x14(%ebp)
    asm volatile (
  802224:	8b 55 f4             	mov    -0xc(%ebp),%edx
  802227:	8b 45 f0             	mov    -0x10(%ebp),%eax
  80222a:	89 d1                	mov    %edx,%ecx
  80222c:	89 c2                	mov    %eax,%edx
  80222e:	89 ce                	mov    %ecx,%esi
  802230:	89 d7                	mov    %edx,%edi
  802232:	ac                   	lods   %ds:(%esi),%al
  802233:	ae                   	scas   %es:(%edi),%al
  802234:	75 08                	jne    80223e <strcmp+0x35>
  802236:	84 c0                	test   %al,%al
  802238:	75 f8                	jne    802232 <strcmp+0x29>
  80223a:	31 c0                	xor    %eax,%eax
  80223c:	eb 04                	jmp    802242 <strcmp+0x39>
  80223e:	19 c0                	sbb    %eax,%eax
  802240:	0c 01                	or     $0x1,%al
  802242:	89 fa                	mov    %edi,%edx
  802244:	89 f1                	mov    %esi,%ecx
  802246:	89 45 ec             	mov    %eax,-0x14(%ebp)
  802249:	89 4d e8             	mov    %ecx,-0x18(%ebp)
  80224c:	89 55 e4             	mov    %edx,-0x1c(%ebp)
    return ret;
  80224f:	8b 45 ec             	mov    -0x14(%ebp),%eax
#ifdef __HAVE_ARCH_STRCMP
    return __strcmp(s1, s2);
  802252:	90                   	nop
    {
        s1 ++, s2 ++;
    }
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
#endif /* __HAVE_ARCH_STRCMP */
}
  802253:	83 c4 20             	add    $0x20,%esp
  802256:	5e                   	pop    %esi
  802257:	5f                   	pop    %edi
  802258:	5d                   	pop    %ebp
  802259:	c3                   	ret    

0080225a <strncmp>:
 * they are equal to each other, it continues with the following pairs until
 * the characters differ, until a terminating null-character is reached, or
 * until @n characters match in both strings, whichever happens first.
 * */
int strncmp(const char *s1, const char *s2, size_t n)
{
  80225a:	55                   	push   %ebp
  80225b:	89 e5                	mov    %esp,%ebp
    while (n > 0 && *s1 != '\0' && *s1 == *s2)
  80225d:	eb 09                	jmp    802268 <strncmp+0xe>
    {
        n --;
  80225f:	ff 4d 10             	decl   0x10(%ebp)
        s1 ++;
  802262:	ff 45 08             	incl   0x8(%ebp)
        s2 ++;
  802265:	ff 45 0c             	incl   0xc(%ebp)
    while (n > 0 && *s1 != '\0' && *s1 == *s2)
  802268:	83 7d 10 00          	cmpl   $0x0,0x10(%ebp)
  80226c:	74 17                	je     802285 <strncmp+0x2b>
  80226e:	8b 45 08             	mov    0x8(%ebp),%eax
  802271:	8a 00                	mov    (%eax),%al
  802273:	84 c0                	test   %al,%al
  802275:	74 0e                	je     802285 <strncmp+0x2b>
  802277:	8b 45 08             	mov    0x8(%ebp),%eax
  80227a:	8a 10                	mov    (%eax),%dl
  80227c:	8b 45 0c             	mov    0xc(%ebp),%eax
  80227f:	8a 00                	mov    (%eax),%al
  802281:	38 c2                	cmp    %al,%dl
  802283:	74 da                	je     80225f <strncmp+0x5>
    }
    return (n == 0) ? 0 : (int)((unsigned char)*s1 - (unsigned char)*s2);
  802285:	83 7d 10 00          	cmpl   $0x0,0x10(%ebp)
  802289:	74 16                	je     8022a1 <strncmp+0x47>
  80228b:	8b 45 08             	mov    0x8(%ebp),%eax
  80228e:	8a 00                	mov    (%eax),%al
  802290:	0f b6 d0             	movzbl %al,%edx
  802293:	8b 45 0c             	mov    0xc(%ebp),%eax
  802296:	8a 00                	mov    (%eax),%al
  802298:	0f b6 c0             	movzbl %al,%eax
  80229b:	29 c2                	sub    %eax,%edx
  80229d:	89 d0                	mov    %edx,%eax
  80229f:	eb 05                	jmp    8022a6 <strncmp+0x4c>
  8022a1:	b8 00 00 00 00       	mov    $0x0,%eax
}
  8022a6:	5d                   	pop    %ebp
  8022a7:	c3                   	ret    

008022a8 <strchr>:
 *
 * The strchr() function returns a pointer to the first occurrence of
 * character in @s. If the value is not found, the function returns 'NULL'.
 * */
char *strchr(const char *s, char c)
{
  8022a8:	55                   	push   %ebp
  8022a9:	89 e5                	mov    %esp,%ebp
  8022ab:	83 ec 04             	sub    $0x4,%esp
  8022ae:	8b 45 0c             	mov    0xc(%ebp),%eax
  8022b1:	88 45 fc             	mov    %al,-0x4(%ebp)
    while (*s != '\0')
  8022b4:	eb 12                	jmp    8022c8 <strchr+0x20>
    {
        if (*s == c)
  8022b6:	8b 45 08             	mov    0x8(%ebp),%eax
  8022b9:	8a 00                	mov    (%eax),%al
  8022bb:	38 45 fc             	cmp    %al,-0x4(%ebp)
  8022be:	75 05                	jne    8022c5 <strchr+0x1d>
        {
            return (char *)s;
  8022c0:	8b 45 08             	mov    0x8(%ebp),%eax
  8022c3:	eb 11                	jmp    8022d6 <strchr+0x2e>
        }
        s ++;
  8022c5:	ff 45 08             	incl   0x8(%ebp)
    while (*s != '\0')
  8022c8:	8b 45 08             	mov    0x8(%ebp),%eax
  8022cb:	8a 00                	mov    (%eax),%al
  8022cd:	84 c0                	test   %al,%al
  8022cf:	75 e5                	jne    8022b6 <strchr+0xe>
    }
    return NULL;
  8022d1:	b8 00 00 00 00       	mov    $0x0,%eax
}
  8022d6:	c9                   	leave  
  8022d7:	c3                   	ret    

008022d8 <strfind>:
 * The strfind() function is like strchr() except that if @c is
 * not found in @s, then it returns a pointer to the null byte at the
 * end of @s, rather than 'NULL'.
 * */
char *strfind(const char *s, char c)
{
  8022d8:	55                   	push   %ebp
  8022d9:	89 e5                	mov    %esp,%ebp
  8022db:	83 ec 04             	sub    $0x4,%esp
  8022de:	8b 45 0c             	mov    0xc(%ebp),%eax
  8022e1:	88 45 fc             	mov    %al,-0x4(%ebp)
    while (*s != '\0')
  8022e4:	eb 0d                	jmp    8022f3 <strfind+0x1b>
    {
        if (*s == c)
  8022e6:	8b 45 08             	mov    0x8(%ebp),%eax
  8022e9:	8a 00                	mov    (%eax),%al
  8022eb:	38 45 fc             	cmp    %al,-0x4(%ebp)
  8022ee:	74 0e                	je     8022fe <strfind+0x26>
        {
            break;
        }
        s ++;
  8022f0:	ff 45 08             	incl   0x8(%ebp)
    while (*s != '\0')
  8022f3:	8b 45 08             	mov    0x8(%ebp),%eax
  8022f6:	8a 00                	mov    (%eax),%al
  8022f8:	84 c0                	test   %al,%al
  8022fa:	75 ea                	jne    8022e6 <strfind+0xe>
  8022fc:	eb 01                	jmp    8022ff <strfind+0x27>
            break;
  8022fe:	90                   	nop
    }
    return (char *)s;
  8022ff:	8b 45 08             	mov    0x8(%ebp),%eax
}
  802302:	c9                   	leave  
  802303:	c3                   	ret    

00802304 <strtol>:
 * an optional "0x" or "0X" prefix.
 *
 * The strtol() function returns the converted integral number as a long int value.
 * */
long strtol(const char *s, char **endptr, int base)
{
  802304:	55                   	push   %ebp
  802305:	89 e5                	mov    %esp,%ebp
  802307:	83 ec 10             	sub    $0x10,%esp
    int neg = 0;
  80230a:	c7 45 fc 00 00 00 00 	movl   $0x0,-0x4(%ebp)
    long val = 0;
  802311:	c7 45 f8 00 00 00 00 	movl   $0x0,-0x8(%ebp)

    // gobble initial whitespace
    while (*s == ' ' || *s == '\t') {
  802318:	eb 03                	jmp    80231d <strtol+0x19>
        s ++;
  80231a:	ff 45 08             	incl   0x8(%ebp)
    while (*s == ' ' || *s == '\t') {
  80231d:	8b 45 08             	mov    0x8(%ebp),%eax
  802320:	8a 00                	mov    (%eax),%al
  802322:	3c 20                	cmp    $0x20,%al
  802324:	74 f4                	je     80231a <strtol+0x16>
  802326:	8b 45 08             	mov    0x8(%ebp),%eax
  802329:	8a 00                	mov    (%eax),%al
  80232b:	3c 09                	cmp    $0x9,%al
  80232d:	74 eb                	je     80231a <strtol+0x16>
    }

    // plus/minus sign
    if (*s == '+') {
  80232f:	8b 45 08             	mov    0x8(%ebp),%eax
  802332:	8a 00                	mov    (%eax),%al
  802334:	3c 2b                	cmp    $0x2b,%al
  802336:	75 05                	jne    80233d <strtol+0x39>
        s ++;
  802338:	ff 45 08             	incl   0x8(%ebp)
  80233b:	eb 13                	jmp    802350 <strtol+0x4c>
    }
    else if (*s == '-') {
  80233d:	8b 45 08             	mov    0x8(%ebp),%eax
  802340:	8a 00                	mov    (%eax),%al
  802342:	3c 2d                	cmp    $0x2d,%al
  802344:	75 0a                	jne    802350 <strtol+0x4c>
        s ++;
  802346:	ff 45 08             	incl   0x8(%ebp)
        neg = 1;
  802349:	c7 45 fc 01 00 00 00 	movl   $0x1,-0x4(%ebp)
    }

    // hex or octal base prefix
    if ((base == 0 || base == 16) && (s[0] == '0' && s[1] == 'x')) {
  802350:	83 7d 10 00          	cmpl   $0x0,0x10(%ebp)
  802354:	74 06                	je     80235c <strtol+0x58>
  802356:	83 7d 10 10          	cmpl   $0x10,0x10(%ebp)
  80235a:	75 20                	jne    80237c <strtol+0x78>
  80235c:	8b 45 08             	mov    0x8(%ebp),%eax
  80235f:	8a 00                	mov    (%eax),%al
  802361:	3c 30                	cmp    $0x30,%al
  802363:	75 17                	jne    80237c <strtol+0x78>
  802365:	8b 45 08             	mov    0x8(%ebp),%eax
  802368:	40                   	inc    %eax
  802369:	8a 00                	mov    (%eax),%al
  80236b:	3c 78                	cmp    $0x78,%al
  80236d:	75 0d                	jne    80237c <strtol+0x78>
        s += 2;
  80236f:	83 45 08 02          	addl   $0x2,0x8(%ebp)
        base = 16;
  802373:	c7 45 10 10 00 00 00 	movl   $0x10,0x10(%ebp)
  80237a:	eb 28                	jmp    8023a4 <strtol+0xa0>
    }
    else if (base == 0 && s[0] == '0') {
  80237c:	83 7d 10 00          	cmpl   $0x0,0x10(%ebp)
  802380:	75 15                	jne    802397 <strtol+0x93>
  802382:	8b 45 08             	mov    0x8(%ebp),%eax
  802385:	8a 00                	mov    (%eax),%al
  802387:	3c 30                	cmp    $0x30,%al
  802389:	75 0c                	jne    802397 <strtol+0x93>
        s ++;
  80238b:	ff 45 08             	incl   0x8(%ebp)
        base = 8;
  80238e:	c7 45 10 08 00 00 00 	movl   $0x8,0x10(%ebp)
  802395:	eb 0d                	jmp    8023a4 <strtol+0xa0>
    }
    else if (base == 0) {
  802397:	83 7d 10 00          	cmpl   $0x0,0x10(%ebp)
  80239b:	75 07                	jne    8023a4 <strtol+0xa0>
        base = 10;
  80239d:	c7 45 10 0a 00 00 00 	movl   $0xa,0x10(%ebp)

    // digits
    while (1) {
        int dig;

        if (*s >= '0' && *s <= '9') {
  8023a4:	8b 45 08             	mov    0x8(%ebp),%eax
  8023a7:	8a 00                	mov    (%eax),%al
  8023a9:	3c 2f                	cmp    $0x2f,%al
  8023ab:	7e 19                	jle    8023c6 <strtol+0xc2>
  8023ad:	8b 45 08             	mov    0x8(%ebp),%eax
  8023b0:	8a 00                	mov    (%eax),%al
  8023b2:	3c 39                	cmp    $0x39,%al
  8023b4:	7f 10                	jg     8023c6 <strtol+0xc2>
            dig = *s - '0';
  8023b6:	8b 45 08             	mov    0x8(%ebp),%eax
  8023b9:	8a 00                	mov    (%eax),%al
  8023bb:	0f be c0             	movsbl %al,%eax
  8023be:	83 e8 30             	sub    $0x30,%eax
  8023c1:	89 45 f4             	mov    %eax,-0xc(%ebp)
  8023c4:	eb 42                	jmp    802408 <strtol+0x104>
        }
        else if (*s >= 'a' && *s <= 'z') {
  8023c6:	8b 45 08             	mov    0x8(%ebp),%eax
  8023c9:	8a 00                	mov    (%eax),%al
  8023cb:	3c 60                	cmp    $0x60,%al
  8023cd:	7e 19                	jle    8023e8 <strtol+0xe4>
  8023cf:	8b 45 08             	mov    0x8(%ebp),%eax
  8023d2:	8a 00                	mov    (%eax),%al
  8023d4:	3c 7a                	cmp    $0x7a,%al
  8023d6:	7f 10                	jg     8023e8 <strtol+0xe4>
            dig = *s - 'a' + 10;
  8023d8:	8b 45 08             	mov    0x8(%ebp),%eax
  8023db:	8a 00                	mov    (%eax),%al
  8023dd:	0f be c0             	movsbl %al,%eax
  8023e0:	83 e8 57             	sub    $0x57,%eax
  8023e3:	89 45 f4             	mov    %eax,-0xc(%ebp)
  8023e6:	eb 20                	jmp    802408 <strtol+0x104>
        }
        else if (*s >= 'A' && *s <= 'Z') {
  8023e8:	8b 45 08             	mov    0x8(%ebp),%eax
  8023eb:	8a 00                	mov    (%eax),%al
  8023ed:	3c 40                	cmp    $0x40,%al
  8023ef:	7e 39                	jle    80242a <strtol+0x126>
  8023f1:	8b 45 08             	mov    0x8(%ebp),%eax
  8023f4:	8a 00                	mov    (%eax),%al
  8023f6:	3c 5a                	cmp    $0x5a,%al
  8023f8:	7f 30                	jg     80242a <strtol+0x126>
            dig = *s - 'A' + 10;
  8023fa:	8b 45 08             	mov    0x8(%ebp),%eax
  8023fd:	8a 00                	mov    (%eax),%al
  8023ff:	0f be c0             	movsbl %al,%eax
  802402:	83 e8 37             	sub    $0x37,%eax
  802405:	89 45 f4             	mov    %eax,-0xc(%ebp)
        }
        else {
            break;
        }
        if (dig >= base) {
  802408:	8b 45 f4             	mov    -0xc(%ebp),%eax
  80240b:	3b 45 10             	cmp    0x10(%ebp),%eax
  80240e:	7d 19                	jge    802429 <strtol+0x125>
            break;
        }
        s ++;
  802410:	ff 45 08             	incl   0x8(%ebp)
        val = (val * base) + dig;
  802413:	8b 45 f8             	mov    -0x8(%ebp),%eax
  802416:	0f af 45 10          	imul   0x10(%ebp),%eax
  80241a:	89 c2                	mov    %eax,%edx
  80241c:	8b 45 f4             	mov    -0xc(%ebp),%eax
  80241f:	01 d0                	add    %edx,%eax
  802421:	89 45 f8             	mov    %eax,-0x8(%ebp)
    while (1) {
  802424:	e9 7b ff ff ff       	jmp    8023a4 <strtol+0xa0>
            break;
  802429:	90                   	nop
        // we don't properly detect overflow!
    }

    if (endptr) {
  80242a:	83 7d 0c 00          	cmpl   $0x0,0xc(%ebp)
  80242e:	74 08                	je     802438 <strtol+0x134>
        *endptr = (char *) s;
  802430:	8b 45 0c             	mov    0xc(%ebp),%eax
  802433:	8b 55 08             	mov    0x8(%ebp),%edx
  802436:	89 10                	mov    %edx,(%eax)
    }
    return (neg ? -val : val);
  802438:	83 7d fc 00          	cmpl   $0x0,-0x4(%ebp)
  80243c:	74 07                	je     802445 <strtol+0x141>
  80243e:	8b 45 f8             	mov    -0x8(%ebp),%eax
  802441:	f7 d8                	neg    %eax
  802443:	eb 03                	jmp    802448 <strtol+0x144>
  802445:	8b 45 f8             	mov    -0x8(%ebp),%eax
}
  802448:	c9                   	leave  
  802449:	c3                   	ret    

0080244a <strtok>:

char *strtok(char *s, const char *demial)
{
  80244a:	55                   	push   %ebp
  80244b:	89 e5                	mov    %esp,%ebp
  80244d:	57                   	push   %edi
  80244e:	53                   	push   %ebx
  80244f:	83 ec 30             	sub    $0x30,%esp
    static unsigned char *last;
    unsigned char *str;
    const unsigned char *ctrl = (const unsigned char *)demial;
  802452:	8b 45 0c             	mov    0xc(%ebp),%eax
  802455:	89 45 f0             	mov    %eax,-0x10(%ebp)
    unsigned char map[32] = {0};
  802458:	8d 55 cc             	lea    -0x34(%ebp),%edx
  80245b:	b9 08 00 00 00       	mov    $0x8,%ecx
  802460:	b8 00 00 00 00       	mov    $0x0,%eax
  802465:	89 d7                	mov    %edx,%edi
  802467:	f3 ab                	rep stos %eax,%es:(%edi)
    int count = 0;
  802469:	c7 45 ec 00 00 00 00 	movl   $0x0,-0x14(%ebp)
    
    for (count = 0; count < 32; count++)
  802470:	c7 45 ec 00 00 00 00 	movl   $0x0,-0x14(%ebp)
  802477:	eb 0e                	jmp    802487 <strtok+0x3d>
    {
        map[count] = 0;
  802479:	8d 55 cc             	lea    -0x34(%ebp),%edx
  80247c:	8b 45 ec             	mov    -0x14(%ebp),%eax
  80247f:	01 d0                	add    %edx,%eax
  802481:	c6 00 00             	movb   $0x0,(%eax)
    for (count = 0; count < 32; count++)
  802484:	ff 45 ec             	incl   -0x14(%ebp)
  802487:	83 7d ec 1f          	cmpl   $0x1f,-0x14(%ebp)
  80248b:	7e ec                	jle    802479 <strtok+0x2f>
    }
    
    do {
        map[*ctrl >> 3] |= (1 << (*ctrl & 7));
  80248d:	8b 45 f0             	mov    -0x10(%ebp),%eax
  802490:	8a 00                	mov    (%eax),%al
  802492:	c0 e8 03             	shr    $0x3,%al
  802495:	0f b6 c0             	movzbl %al,%eax
  802498:	8a 44 05 cc          	mov    -0x34(%ebp,%eax,1),%al
  80249c:	88 c2                	mov    %al,%dl
  80249e:	8b 45 f0             	mov    -0x10(%ebp),%eax
  8024a1:	8a 00                	mov    (%eax),%al
  8024a3:	0f b6 c0             	movzbl %al,%eax
  8024a6:	83 e0 07             	and    $0x7,%eax
  8024a9:	bb 01 00 00 00       	mov    $0x1,%ebx
  8024ae:	88 c1                	mov    %al,%cl
  8024b0:	d3 e3                	shl    %cl,%ebx
  8024b2:	89 d8                	mov    %ebx,%eax
  8024b4:	09 c2                	or     %eax,%edx
  8024b6:	8b 45 f0             	mov    -0x10(%ebp),%eax
  8024b9:	8a 00                	mov    (%eax),%al
  8024bb:	c0 e8 03             	shr    $0x3,%al
  8024be:	0f b6 c0             	movzbl %al,%eax
  8024c1:	88 54 05 cc          	mov    %dl,-0x34(%ebp,%eax,1)
    } while (*ctrl++);
  8024c5:	8b 45 f0             	mov    -0x10(%ebp),%eax
  8024c8:	8d 50 01             	lea    0x1(%eax),%edx
  8024cb:	89 55 f0             	mov    %edx,-0x10(%ebp)
  8024ce:	8a 00                	mov    (%eax),%al
  8024d0:	84 c0                	test   %al,%al
  8024d2:	75 b9                	jne    80248d <strtok+0x43>
    
    if (s)
  8024d4:	83 7d 08 00          	cmpl   $0x0,0x8(%ebp)
  8024d8:	74 08                	je     8024e2 <strtok+0x98>
    {
        str = (unsigned char *)s;
  8024da:	8b 45 08             	mov    0x8(%ebp),%eax
  8024dd:	89 45 f4             	mov    %eax,-0xc(%ebp)
  8024e0:	eb 0d                	jmp    8024ef <strtok+0xa5>
    }
    else
    {
        str = last;
  8024e2:	a1 10 31 80 00       	mov    0x803110,%eax
  8024e7:	89 45 f4             	mov    %eax,-0xc(%ebp)
    }
    
    while ((map[*str >> 3] & (1 << (*str & 7))) && *str)
  8024ea:	eb 03                	jmp    8024ef <strtok+0xa5>
    {
        str++;
  8024ec:	ff 45 f4             	incl   -0xc(%ebp)
    while ((map[*str >> 3] & (1 << (*str & 7))) && *str)
  8024ef:	8b 45 f4             	mov    -0xc(%ebp),%eax
  8024f2:	8a 00                	mov    (%eax),%al
  8024f4:	c0 e8 03             	shr    $0x3,%al
  8024f7:	0f b6 c0             	movzbl %al,%eax
  8024fa:	8a 44 05 cc          	mov    -0x34(%ebp,%eax,1),%al
  8024fe:	0f b6 d0             	movzbl %al,%edx
  802501:	8b 45 f4             	mov    -0xc(%ebp),%eax
  802504:	8a 00                	mov    (%eax),%al
  802506:	0f b6 c0             	movzbl %al,%eax
  802509:	83 e0 07             	and    $0x7,%eax
  80250c:	88 c1                	mov    %al,%cl
  80250e:	d3 fa                	sar    %cl,%edx
  802510:	89 d0                	mov    %edx,%eax
  802512:	83 e0 01             	and    $0x1,%eax
  802515:	85 c0                	test   %eax,%eax
  802517:	74 09                	je     802522 <strtok+0xd8>
  802519:	8b 45 f4             	mov    -0xc(%ebp),%eax
  80251c:	8a 00                	mov    (%eax),%al
  80251e:	84 c0                	test   %al,%al
  802520:	75 ca                	jne    8024ec <strtok+0xa2>
    }
    
    s = (char *)str;
  802522:	8b 45 f4             	mov    -0xc(%ebp),%eax
  802525:	89 45 08             	mov    %eax,0x8(%ebp)
    for (; *str; str++)
  802528:	eb 3b                	jmp    802565 <strtok+0x11b>
    {
        if (map[*str >> 3] & (1 << (*str & 7)))
  80252a:	8b 45 f4             	mov    -0xc(%ebp),%eax
  80252d:	8a 00                	mov    (%eax),%al
  80252f:	c0 e8 03             	shr    $0x3,%al
  802532:	0f b6 c0             	movzbl %al,%eax
  802535:	8a 44 05 cc          	mov    -0x34(%ebp,%eax,1),%al
  802539:	0f b6 d0             	movzbl %al,%edx
  80253c:	8b 45 f4             	mov    -0xc(%ebp),%eax
  80253f:	8a 00                	mov    (%eax),%al
  802541:	0f b6 c0             	movzbl %al,%eax
  802544:	83 e0 07             	and    $0x7,%eax
  802547:	88 c1                	mov    %al,%cl
  802549:	d3 fa                	sar    %cl,%edx
  80254b:	89 d0                	mov    %edx,%eax
  80254d:	83 e0 01             	and    $0x1,%eax
  802550:	85 c0                	test   %eax,%eax
  802552:	74 0e                	je     802562 <strtok+0x118>
        {
            *str++ = '\0';
  802554:	8b 45 f4             	mov    -0xc(%ebp),%eax
  802557:	8d 50 01             	lea    0x1(%eax),%edx
  80255a:	89 55 f4             	mov    %edx,-0xc(%ebp)
  80255d:	c6 00 00             	movb   $0x0,(%eax)
            break;
  802560:	eb 0c                	jmp    80256e <strtok+0x124>
    for (; *str; str++)
  802562:	ff 45 f4             	incl   -0xc(%ebp)
  802565:	8b 45 f4             	mov    -0xc(%ebp),%eax
  802568:	8a 00                	mov    (%eax),%al
  80256a:	84 c0                	test   %al,%al
  80256c:	75 bc                	jne    80252a <strtok+0xe0>
        }
    }
    
    last = str;
  80256e:	8b 45 f4             	mov    -0xc(%ebp),%eax
  802571:	a3 10 31 80 00       	mov    %eax,0x803110
    if (s == (char *)str)
  802576:	8b 45 08             	mov    0x8(%ebp),%eax
  802579:	3b 45 f4             	cmp    -0xc(%ebp),%eax
  80257c:	75 07                	jne    802585 <strtok+0x13b>
    {
        return NULL;
  80257e:	b8 00 00 00 00       	mov    $0x0,%eax
  802583:	eb 03                	jmp    802588 <strtok+0x13e>
    }
    else
    {
        return s;
  802585:	8b 45 08             	mov    0x8(%ebp),%eax
    }
}
  802588:	83 c4 30             	add    $0x30,%esp
  80258b:	5b                   	pop    %ebx
  80258c:	5f                   	pop    %edi
  80258d:	5d                   	pop    %ebp
  80258e:	c3                   	ret    

0080258f <memset>:
 * @n:      number of bytes to be set to the value
 *
 * The memset() function returns @s.
 * */
void *memset(void *s, char c, size_t n)
{
  80258f:	55                   	push   %ebp
  802590:	89 e5                	mov    %esp,%ebp
  802592:	57                   	push   %edi
  802593:	83 ec 24             	sub    $0x24,%esp
  802596:	8b 45 0c             	mov    0xc(%ebp),%eax
  802599:	88 45 d8             	mov    %al,-0x28(%ebp)
#ifdef __HAVE_ARCH_MEMSET
    return __memset(s, c, n);
  80259c:	0f be 45 d8          	movsbl -0x28(%ebp),%eax
  8025a0:	8b 55 08             	mov    0x8(%ebp),%edx
  8025a3:	89 55 f8             	mov    %edx,-0x8(%ebp)
  8025a6:	88 45 f7             	mov    %al,-0x9(%ebp)
  8025a9:	8b 45 10             	mov    0x10(%ebp),%eax
  8025ac:	89 45 f0             	mov    %eax,-0x10(%ebp)
#define __HAVE_ARCH_MEMSET
static inline void *__memset(void *s, char c, size_t n)
{
#if ASM_NO_64
    int d0, d1;
    asm volatile (
  8025af:	8b 4d f0             	mov    -0x10(%ebp),%ecx
  8025b2:	8a 45 f7             	mov    -0x9(%ebp),%al
  8025b5:	8b 55 f8             	mov    -0x8(%ebp),%edx
  8025b8:	89 d7                	mov    %edx,%edi
  8025ba:	f3 aa                	rep stos %al,%es:(%edi)
  8025bc:	89 fa                	mov    %edi,%edx
  8025be:	89 4d ec             	mov    %ecx,-0x14(%ebp)
  8025c1:	89 55 e8             	mov    %edx,-0x18(%ebp)
        "rep; stosb;"
        : "=&c" (d0), "=&D" (d1)
        : "0" (n), "a" (c), "1" (s)
        : "memory");
#endif
    return s;
  8025c4:	8b 45 f8             	mov    -0x8(%ebp),%eax
  8025c7:	90                   	nop
    {
        *p ++ = c;
    }
    return s;
#endif /* __HAVE_ARCH_MEMSET */
}
  8025c8:	83 c4 24             	add    $0x24,%esp
  8025cb:	5f                   	pop    %edi
  8025cc:	5d                   	pop    %ebp
  8025cd:	c3                   	ret    

008025ce <bzero>:

void bzero(void *s, size_t n)
{
  8025ce:	55                   	push   %ebp
  8025cf:	89 e5                	mov    %esp,%ebp
    memset(s, 0,n);
  8025d1:	ff 75 0c             	pushl  0xc(%ebp)
  8025d4:	6a 00                	push   $0x0
  8025d6:	ff 75 08             	pushl  0x8(%ebp)
  8025d9:	e8 b1 ff ff ff       	call   80258f <memset>
  8025de:	83 c4 0c             	add    $0xc,%esp
}
  8025e1:	90                   	nop
  8025e2:	c9                   	leave  
  8025e3:	c3                   	ret    

008025e4 <memmove>:
 * @n:      number of bytes to copy
 *
 * The memmove() function returns @dst.
 * */
void *memmove(void *dst, const void *src, size_t n)
{
  8025e4:	55                   	push   %ebp
  8025e5:	89 e5                	mov    %esp,%ebp
  8025e7:	57                   	push   %edi
  8025e8:	56                   	push   %esi
  8025e9:	53                   	push   %ebx
  8025ea:	83 ec 30             	sub    $0x30,%esp
  8025ed:	8b 45 08             	mov    0x8(%ebp),%eax
  8025f0:	89 45 f0             	mov    %eax,-0x10(%ebp)
  8025f3:	8b 45 0c             	mov    0xc(%ebp),%eax
  8025f6:	89 45 ec             	mov    %eax,-0x14(%ebp)
  8025f9:	8b 45 10             	mov    0x10(%ebp),%eax
  8025fc:	89 45 e8             	mov    %eax,-0x18(%ebp)

#ifndef __HAVE_ARCH_MEMMOVE
#define __HAVE_ARCH_MEMMOVE
static inline void *__memmove(void *dst, const void *src, size_t n)
{
    if (dst < src)
  8025ff:	8b 45 f0             	mov    -0x10(%ebp),%eax
  802602:	3b 45 ec             	cmp    -0x14(%ebp),%eax
  802605:	73 42                	jae    802649 <memmove+0x65>
  802607:	8b 45 f0             	mov    -0x10(%ebp),%eax
  80260a:	89 45 e4             	mov    %eax,-0x1c(%ebp)
  80260d:	8b 45 ec             	mov    -0x14(%ebp),%eax
  802610:	89 45 e0             	mov    %eax,-0x20(%ebp)
  802613:	8b 45 e8             	mov    -0x18(%ebp),%eax
  802616:	89 45 dc             	mov    %eax,-0x24(%ebp)
        "andl $3, %%ecx;"
        "jz 1f;"
        "rep; movsb;"
        "1:"
        : "=&c" (d0), "=&D" (d1), "=&S" (d2)
        : "0" (n / 4), "g" (n), "1" (dst), "2" (src)
  802619:	8b 45 dc             	mov    -0x24(%ebp),%eax
  80261c:	c1 e8 02             	shr    $0x2,%eax
  80261f:	89 c1                	mov    %eax,%ecx
    asm volatile (
  802621:	8b 55 e4             	mov    -0x1c(%ebp),%edx
  802624:	8b 45 e0             	mov    -0x20(%ebp),%eax
  802627:	89 d7                	mov    %edx,%edi
  802629:	89 c6                	mov    %eax,%esi
  80262b:	f3 a5                	rep movsl %ds:(%esi),%es:(%edi)
  80262d:	8b 4d dc             	mov    -0x24(%ebp),%ecx
  802630:	83 e1 03             	and    $0x3,%ecx
  802633:	74 02                	je     802637 <memmove+0x53>
  802635:	f3 a4                	rep movsb %ds:(%esi),%es:(%edi)
  802637:	89 f0                	mov    %esi,%eax
  802639:	89 fa                	mov    %edi,%edx
  80263b:	89 4d d8             	mov    %ecx,-0x28(%ebp)
  80263e:	89 55 d4             	mov    %edx,-0x2c(%ebp)
  802641:	89 45 d0             	mov    %eax,-0x30(%ebp)
        : "memory");
#endif
    return dst;
  802644:	8b 45 e4             	mov    -0x1c(%ebp),%eax
#ifdef __HAVE_ARCH_MEMMOVE
    return __memmove(dst, src, n);
  802647:	eb 36                	jmp    80267f <memmove+0x9b>
        : "0" (n), "1" (n - 1 + src), "2" (n - 1 + dst)
  802649:	8b 45 e8             	mov    -0x18(%ebp),%eax
  80264c:	8d 50 ff             	lea    -0x1(%eax),%edx
  80264f:	8b 45 ec             	mov    -0x14(%ebp),%eax
  802652:	01 c2                	add    %eax,%edx
  802654:	8b 45 e8             	mov    -0x18(%ebp),%eax
  802657:	8d 48 ff             	lea    -0x1(%eax),%ecx
  80265a:	8b 45 f0             	mov    -0x10(%ebp),%eax
  80265d:	8d 1c 01             	lea    (%ecx,%eax,1),%ebx
    asm volatile (
  802660:	8b 45 e8             	mov    -0x18(%ebp),%eax
  802663:	89 c1                	mov    %eax,%ecx
  802665:	89 d8                	mov    %ebx,%eax
  802667:	89 d6                	mov    %edx,%esi
  802669:	89 c7                	mov    %eax,%edi
  80266b:	fd                   	std    
  80266c:	f3 a4                	rep movsb %ds:(%esi),%es:(%edi)
  80266e:	fc                   	cld    
  80266f:	89 f8                	mov    %edi,%eax
  802671:	89 f2                	mov    %esi,%edx
  802673:	89 4d cc             	mov    %ecx,-0x34(%ebp)
  802676:	89 55 c8             	mov    %edx,-0x38(%ebp)
  802679:	89 45 c4             	mov    %eax,-0x3c(%ebp)
    return dst;
  80267c:	8b 45 f0             	mov    -0x10(%ebp),%eax
            *d ++ = *s ++;
        }
    }
    return dst;
#endif /* __HAVE_ARCH_MEMMOVE */
}
  80267f:	83 c4 30             	add    $0x30,%esp
  802682:	5b                   	pop    %ebx
  802683:	5e                   	pop    %esi
  802684:	5f                   	pop    %edi
  802685:	5d                   	pop    %ebp
  802686:	c3                   	ret    

00802687 <memcpy>:
 * it always copies exactly @n bytes. To avoid overflows, the size of arrays pointed
 * by both @src and @dst, should be at least @n bytes, and should not overlap
 * (for overlapping memory area, memmove is a safer approach).
 * */
void *memcpy(void *dst, const void *src, size_t n)
{
  802687:	55                   	push   %ebp
  802688:	89 e5                	mov    %esp,%ebp
  80268a:	57                   	push   %edi
  80268b:	56                   	push   %esi
  80268c:	83 ec 20             	sub    $0x20,%esp
  80268f:	8b 45 08             	mov    0x8(%ebp),%eax
  802692:	89 45 f4             	mov    %eax,-0xc(%ebp)
  802695:	8b 45 0c             	mov    0xc(%ebp),%eax
  802698:	89 45 f0             	mov    %eax,-0x10(%ebp)
  80269b:	8b 45 10             	mov    0x10(%ebp),%eax
  80269e:	89 45 ec             	mov    %eax,-0x14(%ebp)
        : "0" (n / 4), "g" (n), "1" (dst), "2" (src)
  8026a1:	8b 45 ec             	mov    -0x14(%ebp),%eax
  8026a4:	c1 e8 02             	shr    $0x2,%eax
  8026a7:	89 c1                	mov    %eax,%ecx
    asm volatile (
  8026a9:	8b 55 f4             	mov    -0xc(%ebp),%edx
  8026ac:	8b 45 f0             	mov    -0x10(%ebp),%eax
  8026af:	89 d7                	mov    %edx,%edi
  8026b1:	89 c6                	mov    %eax,%esi
  8026b3:	f3 a5                	rep movsl %ds:(%esi),%es:(%edi)
  8026b5:	8b 4d ec             	mov    -0x14(%ebp),%ecx
  8026b8:	83 e1 03             	and    $0x3,%ecx
  8026bb:	74 02                	je     8026bf <memcpy+0x38>
  8026bd:	f3 a4                	rep movsb %ds:(%esi),%es:(%edi)
  8026bf:	89 f0                	mov    %esi,%eax
  8026c1:	89 fa                	mov    %edi,%edx
  8026c3:	89 4d e8             	mov    %ecx,-0x18(%ebp)
  8026c6:	89 55 e4             	mov    %edx,-0x1c(%ebp)
  8026c9:	89 45 e0             	mov    %eax,-0x20(%ebp)
    return dst;
  8026cc:	8b 45 f4             	mov    -0xc(%ebp),%eax
#ifdef __HAVE_ARCH_MEMCPY
    return __memcpy(dst, src, n);
  8026cf:	90                   	nop
    {
        *d ++ = *s ++;
    }
    return dst;
#endif /* __HAVE_ARCH_MEMCPY */
}
  8026d0:	83 c4 20             	add    $0x20,%esp
  8026d3:	5e                   	pop    %esi
  8026d4:	5f                   	pop    %edi
  8026d5:	5d                   	pop    %ebp
  8026d6:	c3                   	ret    

008026d7 <memcmp>:
 *   match in both memory blocks has a greater value in @v1 than in @v2
 *   as if evaluated as unsigned char values;
 * - And a value less than zero indicates the opposite.
 * */
int memcmp(const void *v1, const void *v2, size_t n)
{
  8026d7:	55                   	push   %ebp
  8026d8:	89 e5                	mov    %esp,%ebp
  8026da:	83 ec 10             	sub    $0x10,%esp
    const char *s1 = (const char *)v1;
  8026dd:	8b 45 08             	mov    0x8(%ebp),%eax
  8026e0:	89 45 fc             	mov    %eax,-0x4(%ebp)
    const char *s2 = (const char *)v2;
  8026e3:	8b 45 0c             	mov    0xc(%ebp),%eax
  8026e6:	89 45 f8             	mov    %eax,-0x8(%ebp)
    while (n -- > 0)
  8026e9:	eb 2a                	jmp    802715 <memcmp+0x3e>
    {
        if (*s1 != *s2)
  8026eb:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8026ee:	8a 10                	mov    (%eax),%dl
  8026f0:	8b 45 f8             	mov    -0x8(%ebp),%eax
  8026f3:	8a 00                	mov    (%eax),%al
  8026f5:	38 c2                	cmp    %al,%dl
  8026f7:	74 16                	je     80270f <memcmp+0x38>
        {
            return (int)((unsigned char)*s1 - (unsigned char)*s2);
  8026f9:	8b 45 fc             	mov    -0x4(%ebp),%eax
  8026fc:	8a 00                	mov    (%eax),%al
  8026fe:	0f b6 d0             	movzbl %al,%edx
  802701:	8b 45 f8             	mov    -0x8(%ebp),%eax
  802704:	8a 00                	mov    (%eax),%al
  802706:	0f b6 c0             	movzbl %al,%eax
  802709:	29 c2                	sub    %eax,%edx
  80270b:	89 d0                	mov    %edx,%eax
  80270d:	eb 18                	jmp    802727 <memcmp+0x50>
        }
        s1 ++;
  80270f:	ff 45 fc             	incl   -0x4(%ebp)
        s2 ++;
  802712:	ff 45 f8             	incl   -0x8(%ebp)
    while (n -- > 0)
  802715:	8b 45 10             	mov    0x10(%ebp),%eax
  802718:	8d 50 ff             	lea    -0x1(%eax),%edx
  80271b:	89 55 10             	mov    %edx,0x10(%ebp)
  80271e:	85 c0                	test   %eax,%eax
  802720:	75 c9                	jne    8026eb <memcmp+0x14>
    }
    return 0;
  802722:	b8 00 00 00 00       	mov    $0x0,%eax
}
  802727:	c9                   	leave  
  802728:	c3                   	ret    

00802729 <index>:
/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found
 */
char *index(char *sp, char c)
{
  802729:	55                   	push   %ebp
  80272a:	89 e5                	mov    %esp,%ebp
  80272c:	83 ec 04             	sub    $0x4,%esp
  80272f:	8b 45 0c             	mov    0xc(%ebp),%eax
  802732:	88 45 fc             	mov    %al,-0x4(%ebp)
    do {
        if (*sp == c)
  802735:	8b 45 08             	mov    0x8(%ebp),%eax
  802738:	8a 00                	mov    (%eax),%al
  80273a:	38 45 fc             	cmp    %al,-0x4(%ebp)
  80273d:	75 05                	jne    802744 <index+0x1b>
            return (sp);
  80273f:	8b 45 08             	mov    0x8(%ebp),%eax
  802742:	eb 14                	jmp    802758 <index+0x2f>
    } while (*sp++);
  802744:	8b 45 08             	mov    0x8(%ebp),%eax
  802747:	8d 50 01             	lea    0x1(%eax),%edx
  80274a:	89 55 08             	mov    %edx,0x8(%ebp)
  80274d:	8a 00                	mov    (%eax),%al
  80274f:	84 c0                	test   %al,%al
  802751:	75 e2                	jne    802735 <index+0xc>
    
    return(NULL);
  802753:	b8 00 00 00 00       	mov    $0x0,%eax
}
  802758:	c9                   	leave  
  802759:	c3                   	ret    

0080275a <atoi>:

int atoi(const char *p)
{
  80275a:	55                   	push   %ebp
  80275b:	89 e5                	mov    %esp,%ebp
  80275d:	56                   	push   %esi
  80275e:	53                   	push   %ebx
    register int n;
    register int f;

    n = 0;
  80275f:	bb 00 00 00 00       	mov    $0x0,%ebx
    f = 0;
  802764:	be 00 00 00 00       	mov    $0x0,%esi
    for(; ; p++)
    {
        switch(*p)
  802769:	8b 45 08             	mov    0x8(%ebp),%eax
  80276c:	8a 00                	mov    (%eax),%al
  80276e:	0f be c0             	movsbl %al,%eax
  802771:	83 f8 20             	cmp    $0x20,%eax
  802774:	74 18                	je     80278e <atoi+0x34>
  802776:	83 f8 20             	cmp    $0x20,%eax
  802779:	7f 07                	jg     802782 <atoi+0x28>
  80277b:	83 f8 09             	cmp    $0x9,%eax
  80277e:	74 0e                	je     80278e <atoi+0x34>
            case '-':
                f++;
            case '+':
                p++;
        }
        break;
  802780:	eb 15                	jmp    802797 <atoi+0x3d>
  802782:	83 f8 2b             	cmp    $0x2b,%eax
  802785:	74 0d                	je     802794 <atoi+0x3a>
  802787:	83 f8 2d             	cmp    $0x2d,%eax
  80278a:	74 07                	je     802793 <atoi+0x39>
  80278c:	eb 09                	jmp    802797 <atoi+0x3d>
    for(; ; p++)
  80278e:	ff 45 08             	incl   0x8(%ebp)
        switch(*p)
  802791:	eb d6                	jmp    802769 <atoi+0xf>
                f++;
  802793:	46                   	inc    %esi
                p++;
  802794:	ff 45 08             	incl   0x8(%ebp)
        break;
  802797:	90                   	nop
    }
    
    while(*p >= '0' && *p <= '9')
  802798:	eb 1e                	jmp    8027b8 <atoi+0x5e>
        n = n * 10 + *p++ - '0';
  80279a:	89 d8                	mov    %ebx,%eax
  80279c:	c1 e0 02             	shl    $0x2,%eax
  80279f:	01 d8                	add    %ebx,%eax
  8027a1:	01 c0                	add    %eax,%eax
  8027a3:	89 c1                	mov    %eax,%ecx
  8027a5:	8b 45 08             	mov    0x8(%ebp),%eax
  8027a8:	8d 50 01             	lea    0x1(%eax),%edx
  8027ab:	89 55 08             	mov    %edx,0x8(%ebp)
  8027ae:	8a 00                	mov    (%eax),%al
  8027b0:	0f be c0             	movsbl %al,%eax
  8027b3:	01 c8                	add    %ecx,%eax
  8027b5:	8d 58 d0             	lea    -0x30(%eax),%ebx
    while(*p >= '0' && *p <= '9')
  8027b8:	8b 45 08             	mov    0x8(%ebp),%eax
  8027bb:	8a 00                	mov    (%eax),%al
  8027bd:	3c 2f                	cmp    $0x2f,%al
  8027bf:	7e 09                	jle    8027ca <atoi+0x70>
  8027c1:	8b 45 08             	mov    0x8(%ebp),%eax
  8027c4:	8a 00                	mov    (%eax),%al
  8027c6:	3c 39                	cmp    $0x39,%al
  8027c8:	7e d0                	jle    80279a <atoi+0x40>
    
    return(f? -n: n);
  8027ca:	85 f6                	test   %esi,%esi
  8027cc:	74 06                	je     8027d4 <atoi+0x7a>
  8027ce:	89 d8                	mov    %ebx,%eax
  8027d0:	f7 d8                	neg    %eax
  8027d2:	eb 02                	jmp    8027d6 <atoi+0x7c>
  8027d4:	89 d8                	mov    %ebx,%eax
}
  8027d6:	5b                   	pop    %ebx
  8027d7:	5e                   	pop    %esi
  8027d8:	5d                   	pop    %ebp
  8027d9:	c3                   	ret    

008027da <blkequ>:

bool blkequ(void* first, void* second, int nbytes)
{
  8027da:	55                   	push   %ebp
  8027db:	89 e5                	mov    %esp,%ebp
    return (0 == memcmp(first, second, nbytes)) ? 1 : 0;
  8027dd:	8b 45 10             	mov    0x10(%ebp),%eax
  8027e0:	50                   	push   %eax
  8027e1:	ff 75 0c             	pushl  0xc(%ebp)
  8027e4:	ff 75 08             	pushl  0x8(%ebp)
  8027e7:	e8 eb fe ff ff       	call   8026d7 <memcmp>
  8027ec:	83 c4 0c             	add    $0xc,%esp
  8027ef:	85 c0                	test   %eax,%eax
  8027f1:	0f 94 c0             	sete   %al
  8027f4:	0f b6 c0             	movzbl %al,%eax
}
  8027f7:	c9                   	leave  
  8027f8:	c3                   	ret    

008027f9 <main>:
#define BUFSIZE                         4096

// 这里 pwd 执行 getcwd 虽然获取是的当前 pwd 进程的目录，其实也是进程 sh 的目录
// 因为 pwd 的父进程是 sh，进程 fork 的时候 pwd 复制了 sh 进程当前的目录
int main(int argc, char **argv)
{
  8027f9:	8d 4c 24 04          	lea    0x4(%esp),%ecx
  8027fd:	83 e4 f0             	and    $0xfffffff0,%esp
  802800:	ff 71 fc             	pushl  -0x4(%ecx)
  802803:	55                   	push   %ebp
  802804:	89 e5                	mov    %esp,%ebp
  802806:	51                   	push   %ecx
  802807:	83 ec 14             	sub    $0x14,%esp
    int ret;
    static char cwdbuf[BUFSIZE];
    if ((ret = getcwd(cwdbuf, sizeof(cwdbuf))) != 0)
  80280a:	83 ec 08             	sub    $0x8,%esp
  80280d:	68 00 10 00 00       	push   $0x1000
  802812:	68 20 31 80 00       	push   $0x803120
  802817:	e8 0f d9 ff ff       	call   80012b <getcwd>
  80281c:	83 c4 10             	add    $0x10,%esp
  80281f:	89 45 f4             	mov    %eax,-0xc(%ebp)
  802822:	83 7d f4 00          	cmpl   $0x0,-0xc(%ebp)
  802826:	74 05                	je     80282d <main+0x34>
    {
        return ret;
  802828:	8b 45 f4             	mov    -0xc(%ebp),%eax
  80282b:	eb 1c                	jmp    802849 <main+0x50>
    }
    printf("current dir is [%s].\n", cwdbuf);
  80282d:	83 ec 04             	sub    $0x4,%esp
  802830:	68 20 31 80 00       	push   $0x803120
  802835:	68 d4 2d 80 00       	push   $0x802dd4
  80283a:	6a 01                	push   $0x1
  80283c:	e8 13 e3 ff ff       	call   800b54 <fprintf>
  802841:	83 c4 10             	add    $0x10,%esp
	return 0;
  802844:	b8 00 00 00 00       	mov    $0x0,%eax
}
  802849:	8b 4d fc             	mov    -0x4(%ebp),%ecx
  80284c:	c9                   	leave  
  80284d:	8d 61 fc             	lea    -0x4(%ecx),%esp
  802850:	c3                   	ret    
