#include "defs.h"
#include "stdio.h"
#include "trap.h"
#include "picirq.h"
#include "fs.h"
#include "ide.h"
#include "x86.h"
#include "assert.h"
#include "cpu.h"
#include "dma.h"

#define ISA_DATA                0x00
#define ISA_ERROR               0x01
#define ISA_PRECOMP             0x01
#define ISA_CTRL                0x02
#define ISA_SECCNT              0x02
#define ISA_SECTOR              0x03
#define ISA_CYL_LO              0x04
#define ISA_CYL_HI              0x05
#define ISA_SDH                 0x06
#define ISA_COMMAND             0x07
#define ISA_STATUS              0x07

#define IDE_BSY                 0x80
#define IDE_DRDY                0x40
#define IDE_DF                  0x20
#define IDE_DRQ                 0x08
#define IDE_ERR                 0x01

#define IDE_CMD_READ            0x20
#define IDE_CMD_WRITE           0x30
#define IDE_CMD_IDENTIFY        0xEC

#define IDE_IDENT_SECTORS       20
#define IDE_IDENT_MODEL         54
#define IDE_IDENT_CAPABILITIES  98
#define IDE_IDENT_CMDSETS       164
#define IDE_IDENT_MAX_LBA       120
#define IDE_IDENT_MAX_LBA_EXT   200

/*
 端口地址参考 cat /proc/ioports
 170H　　      1号硬盘数据寄存器
 171H　　      1号硬盘错误寄存器
 172H　　      1号硬盘数据扇区计数
 173H　　      1号硬盘扇区数
 174H　　      1号硬盘柱面（低字节）
 175H　　      1号硬盘柱面（高字节）
 176H　　      1号硬盘驱动器/磁头寄存器
 177H　　      1号硬盘状态寄存器
 1F0H　　      0号硬盘数据寄存器
 1F1H　　      0号硬盘错误寄存器
 1F2H　　      0号硬盘数据扇区计数
 1F3H　　      0号硬盘扇区数
 1F4H　　      0号硬盘柱面（低字节）
 1F5H　　      0号硬盘柱面（高字节）
 1F6H　　      0号硬盘驱动器/磁头寄存器
 1F7H　　      0号硬盘状态寄存器
*/
#define IO_BASE0                0x1F0
#define IO_BASE1                0x170
#define IO_CTRL0                0x3F4
#define IO_CTRL1                0x374

#define MAX_IDE                 4
#define MAX_NSECS               128
#define MAX_DISK_NSECS          0x10000000U
#define VALID_IDE(ideno)        (((ideno) >= 0) && ((ideno) < MAX_IDE) && (ide_devices[ideno].valid))

static const struct {
    unsigned short base;        // I/O Base
    unsigned short ctrl;        // Control Base
} channels[2] = {
    {IO_BASE0, IO_CTRL0},
    {IO_BASE1, IO_CTRL1},
};

#define IO_BASE(ideno)          (channels[(ideno) >> 1].base)
#define IO_CTRL(ideno)          (channels[(ideno) >> 1].ctrl)

/*
 SECTSIZE   512
 ide 0:      10000(sectors), 'QEMU HARDDISK'.   10000 * 512 = 5.1 M
 ide 1:     262144(sectors), 'QEMU HARDDISK'.   262144 * 512 = 134.2 M
 ide 2:     262144(sectors), 'QEMU HARDDISK'.   262144 * 512 = 134.2 M
 ide 3:     262144(sectors), 'QEMU HARDDISK'.   262144 * 512 = 134.2 M
*/
/*
 {
    {
        valid = 0x1,
        sets = 0x74004021,
        size = 0x2710,
        model = {"QEMU HARDDISK"}
    },
    {
        valid = 0x1,
        sets = 0x74004021,
        size = 0x40000,
        model = {"QEMU HARDDISK"}
    },
    {
        valid = 0x1,
        sets = 0x74004021,
        size = 0x40000,
        model = {"QEMU HARDDISK"}
    },
    {
        valid = 0x1,
        sets = 0x74004021,
        size = 0x40000,
        model = {"QEMU HARDDISK"}
    }
 }
*/
static struct ide_device
{
    unsigned char valid;        // 0 or 1 (If Device Really Exists)
    unsigned int sets;          // Commend Sets Supported
    unsigned int size;          // Size in Sectors 扇区数
    unsigned char model[41];    // Model in String
} ide_devices[MAX_IDE];

// 这里还是 cpu while 循环等待磁盘是否准备好，耗费 cpu，效率低
static int ide_wait_ready(unsigned short iobase, bool check_error)
{
    int r;
    while ((r = inb(iobase + ISA_STATUS)) & IDE_BSY)
        /* nothing */;
    if (check_error && (r & (IDE_DF | IDE_ERR)) != 0)
    {
        return -1;
    }
    return 0;
}

void ide_init(void)
{
    static_assert((SECTSIZE % 4) == 0);
    unsigned short ideno, iobase;
    for (ideno = 0; ideno < MAX_IDE; ideno ++)
    {
        /* assume that no device here */
        ide_devices[ideno].valid = 0;

        iobase = IO_BASE(ideno);

        /* wait device ready */
        ide_wait_ready(iobase, 0);

        /* step1: select drive */
        outb(iobase + ISA_SDH, 0xE0 | ((ideno & 1) << 4));
        ide_wait_ready(iobase, 0);

        /* step2: send ATA identify command */
        outb(iobase + ISA_COMMAND, IDE_CMD_IDENTIFY);
        ide_wait_ready(iobase, 0);

        /* step3: polling */
        if (inb(iobase + ISA_STATUS) == 0 || ide_wait_ready(iobase, 1) != 0)
        {
            continue ;
        }

        /* device is ok */
        ide_devices[ideno].valid = 1;

        /* read identification space of the device */
        unsigned int buffer[128];
        insl(iobase + ISA_DATA, buffer, sizeof(buffer) / sizeof(unsigned int));

        unsigned char *ident = (unsigned char *)buffer;
        unsigned int sectors;
        unsigned int cmdsets = *(unsigned int *)(ident + IDE_IDENT_CMDSETS);
        /* device use 48-bits or 28-bits addressing */
        if (cmdsets & (1 << 26))
        {
            sectors = *(unsigned int *)(ident + IDE_IDENT_MAX_LBA_EXT);
        }
        else
        {
            sectors = *(unsigned int *)(ident + IDE_IDENT_MAX_LBA);
        }
        ide_devices[ideno].sets = cmdsets;  // 0x74004021
        ide_devices[ideno].size = sectors;  // 0x2710

        /* check if supports LBA */
        assert((*(unsigned short *)(ident + IDE_IDENT_CAPABILITIES) & 0x200) != 0);

        unsigned char *model = ide_devices[ideno].model, *data = ident + IDE_IDENT_MODEL;
        unsigned int i, length = 40;
        for (i = 0; i < length; i += 2)
        {
            model[i] = data[i + 1];
            model[i + 1] = data[i];
        }
        do {
            model[i] = '\0';
        } while (i -- > 0 && model[i] == ' ');

        cprintf("ide %d: %10u(sectors), '%s'.\n", ideno, ide_devices[ideno].size, ide_devices[ideno].model);
    }

    // enable ide interrupt
    irq_setmask_8259A(irq_mask_8259A & ~(1 << IRQ_IDE1));
    irq_setmask_8259A(irq_mask_8259A & ~(1 << IRQ_IDE2));
    
    ioapic_enable(IRQ_IDE1, 0);
    ioapic_enable(IRQ_IDE2, 0);
}

bool ide_device_valid(unsigned short ideno)
{
    return VALID_IDE(ideno);
}

size_t ide_device_size(unsigned short ideno)
{
    if (ide_device_valid(ideno))
    {
        return ide_devices[ideno].size;
    }
    return 0;
}

/*
 从设备上读取数据
 ideno  设备号
 secno  起始扇区号
 dst    读取数据到什么地址
 nsecs  要读取的扇区数
*/
int ide_read_secs(unsigned short ideno, uint32_t secno, void *dst, size_t nsecs)
{
    assert(nsecs <= MAX_NSECS && VALID_IDE(ideno));
    assert(secno < MAX_DISK_NSECS && secno + nsecs <= MAX_DISK_NSECS);
    unsigned short iobase = IO_BASE(ideno), ioctrl = IO_CTRL(ideno);

    
#define KBD_DMA 2
    if ((unsigned int)dst - KERNBASE < 0x1000000 - 1) /* transfer to address < 16M? */
    {
        if (!request_dma(KBD_DMA))
        {
            disable_dma(KBD_DMA);
            clear_dma_ff(KBD_DMA);
            set_dma_mode(KBD_DMA, DMA_MODE_READ);
            set_dma_addr(KBD_DMA, (unsigned int)dst - KERNBASE);
            set_dma_count(KBD_DMA, 1);
            enable_dma(KBD_DMA);
        }
    }
    
    ide_wait_ready(iobase, 0);

    // generate interrupt
    outb(ioctrl + ISA_CTRL, 0);
    outb(iobase + ISA_SECCNT, nsecs);
    outb(iobase + ISA_SECTOR, secno & 0xFF);
    outb(iobase + ISA_CYL_LO, (secno >> 8) & 0xFF);
    outb(iobase + ISA_CYL_HI, (secno >> 16) & 0xFF);
    outb(iobase + ISA_SDH, 0xE0 | ((ideno & 1) << 4) | ((secno >> 24) & 0xF));
    outb(iobase + ISA_COMMAND, IDE_CMD_READ);

    // 这里是 cpu 去循环从设备读取数据，一个一个扇区读出，读完一个扇区还要检测硬盘是否 ready
    // 受设备速度限制，读取性能很低，优化方式是 DMA
    int ret = 0;
    for (; nsecs > 0; nsecs --, dst += SECTSIZE)
    {
        if ((ret = ide_wait_ready(iobase, 1)) != 0)
        {
            goto out;
        }
        insl(iobase, dst, SECTSIZE / sizeof(uint32_t));
    }

out:
    return ret;
}

/*
 写入数据到设备上
 ideno  设备号
 secno  起始扇区号
 src    待写入的数据来源
 nsecs  要写入的扇区数
 */
int ide_write_secs(unsigned short ideno, uint32_t secno, const void *src, size_t nsecs)
{
    assert(nsecs <= MAX_NSECS && VALID_IDE(ideno));
    assert(secno < MAX_DISK_NSECS && secno + nsecs <= MAX_DISK_NSECS);
    unsigned short iobase = IO_BASE(ideno), ioctrl = IO_CTRL(ideno);

    ide_wait_ready(iobase, 0);

    // generate interrupt
    outb(ioctrl + ISA_CTRL, 0);
    outb(iobase + ISA_SECCNT, nsecs);
    outb(iobase + ISA_SECTOR, secno & 0xFF);
    outb(iobase + ISA_CYL_LO, (secno >> 8) & 0xFF);
    outb(iobase + ISA_CYL_HI, (secno >> 16) & 0xFF);
    outb(iobase + ISA_SDH, 0xE0 | ((ideno & 1) << 4) | ((secno >> 24) & 0xF));
    outb(iobase + ISA_COMMAND, IDE_CMD_WRITE);

    // 这里是 cpu 去循环写入到设备，一个一个扇区写入，写完一个扇区还要检测硬盘是否 ready
    // 受设备速度限制，写入性能很低，优化方式是 DMA
    int ret = 0;
    for (; nsecs > 0; nsecs --, src += SECTSIZE)
    {
        if ((ret = ide_wait_ready(iobase, 1)) != 0)
        {
            goto out;
        }
        outsl(iobase, src, SECTSIZE / sizeof(uint32_t));
    }

out:
    return ret;
}

