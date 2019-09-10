#include "defs.h"
#include "x86.h"
#include "stdio.h"
#include "string.h"
#include "kbdreg.h"
#include "picirq.h"
#include "trap.h"
#include "memlayout.h"
#include "sync.h"
#include "cpu.h"
#include "dma.h"

/* 为什么显卡在内存中的映射是 0xB8000 ?
 https://www.zhihu.com/question/269649445/answer/351632444?hb_wx_block=0&utm_source=wechat_session&utm_medium=social&utm_oi=638074379922313216
 A0000~AFFFF: VGA 图形模式显存空间
 B0000~B7FFF: MDA 单色字符模式显存空间
 B8000~BFFFF: CGA 彩色字符模式显存空间
 C0000~C7FFF: 显卡 ROM 空间（后来被改造成多种用途，也可以映射显存）
 C8000~FFFFE: 留给 BIOS 以及其它硬件使用（比如硬盘 ROM 之类的）。
*/

/* stupid I/O delay routine necessitated by historical PC design flaws */
static void delay(void)
{
    inb(0x84);
    inb(0x84);
    inb(0x84);
    inb(0x84);
}

/***** Serial I/O code *****/
/*
 端口地址参考 cat /proc/ioports
 3F8H　　　    1号串行口，发送/保持寄存器(RS232接口卡1)
 3F9H　　　    1号串行口，中断有效寄存器
 3FAH　　　    1号串行口，中断ID寄存器
 3FBH　　　    1号串行口，线控制寄存器
 3FCH　　　    1号串行口，调制解调控制寄存器
 3FDH　　　    1号串行口，线状态寄存器
 3FEH　　　    1号串行口，调制解调状态寄存器
*/
#define COM1            0x3F8

#define COM_RX          0       // In:  Receive buffer (DLAB=0)
#define COM_TX          0       // Out: Transmit buffer (DLAB=0)
#define COM_DLL         0       // Out: Divisor Latch Low (DLAB=1)
#define COM_DLM         1       // Out: Divisor Latch High (DLAB=1)
#define COM_IER         1       // Out: Interrupt Enable Register
#define COM_IER_RDI     0x01    // Enable receiver data interrupt
#define COM_IIR         2       // In:  Interrupt ID Register
#define COM_FCR         2       // Out: FIFO Control Register
#define COM_LCR         3       // Out: Line Control Register
#define COM_LCR_DLAB    0x80    // Divisor latch access bit
#define COM_LCR_WLEN8   0x03    // Wordlength: 8 bits
#define COM_MCR         4       // Out: Modem Control Register
#define COM_MCR_RTS     0x02    // RTS complement
#define COM_MCR_DTR     0x01    // DTR complement
#define COM_MCR_OUT2    0x08    // Out2 complement
#define COM_LSR         5       // In:  Line Status Register
#define COM_LSR_DATA    0x01    // Data available
#define COM_LSR_TXRDY   0x20    // Transmit buffer avail
#define COM_LSR_TSRE    0x40    // Transmitter off

// 端口地址参考 cat /proc/ioports
// 3B0H-3BFH　  MDA视频寄存器
// 3D0H-3D7H　  CGA视频寄存器
// 显示器初始化，CGA 是 Color Graphics Adapter 的缩写
// CGA 显存按照下面的方式映射：
// -- 0xB0000 - 0xB7FFF 单色字符模式
// -- 0xB8000 - 0xBFFFF 彩色字符模式及 CGA 兼容图形模式
// 6845 芯片是 IBM PC 中的视频控制器
// CPU 通过 IO 地址 0x3B4-0x3B5 来驱动 6845 控制单色显示，通过 IO 地址
// 0x3D4-0x3D5 来控制彩色显示。
// -- 数据寄存器 映射 到 端口 0x3D5 或 0x3B5
// -- 索引寄存器 0x3D4 或 0x3B4, 决定在数据寄存器中的数据表示什么。
#define MONO_BASE       0x3B4
#define MONO_BUF        0xB0000
#define CGA_BASE        0x3D4
#define CGA_BUF         0xB8000 // MMIO address
#define CRT_ROWS        25
#define CRT_COLS        80
#define CRT_SIZE        (CRT_ROWS * CRT_COLS)

/*
 端口地址参考 cat /proc/ioports
 378H　　　    2号并行口，数据端口
 379H　　　    2号并行口，状态端口
 37AH　　　    2号并行口，控制端口
*/
#define LPTPORT         0x378

// crt_buf 是 CGA 显存起始地址
static uint16_t *crt_buf;
// crt_pos 是 CGA 当前光标位置
static uint16_t crt_pos;
static uint16_t addr_6845;

/* TEXT-mode CGA/VGA display output */
/*
 CPU 通过 MMIO 的方式来驱动显卡的 VGA text mode。
 具体规则是在内存地址 0xB8000（这是一个 MMIO 地址） 处有一个 25 * 80，单个元素 2 字节
 大小的二维数组（VGA text buffer），对应显示区域的 25 行 80 列的显示区域。
 每个数组元素就是一个单元格内的显示数据。程序只要指定读写该区域的数据，就能实现显示控制。
 
 Volatile 的原意是挥发性的，不稳定。而 Volatile variable 指的是那些可能通过多种途径进行
 修改的数据。比如下面的 CGA_BUF，如果在程序中使用那么指针对应的数据，除了程序本身会修改，
 其他的程序也会修改，显卡驱动（bios?）也会修改。
 
 这样的数据会有一个问题，就是很多时候对于单一程序只会进行读取/写入这种操作中的一种，比如
 VGA text buffer 读取很多时候是显卡的事，程序则只负责写入。这样的数据，
 编译器会认为是没有意义的，很可能会在优化的时候被去掉，反而出现错误。所以一些语言中会把这样
 的数据标记出来。MMIO 是 volatile 最常用的场景之一。
*/
static void cga_init(void)
{
    // CGA_BUF: 0xB8000 (彩色显示的显存物理基址)
    volatile uint16_t *cp = (uint16_t *)(CGA_BUF + KERNBASE);
    // 保存当前显存 0xB8000 处的值
    uint16_t was = *cp;
    // 给这个地址随便写个值，看看能否再读出同样的值
    *cp = (uint16_t) 0xA55A;
    if (*cp != 0xA55A)
    {
        // 如果读不出来，说明没有这块显存，即是单显配置
        // 设置为单显的显存基址 MONO_BUF： 0xB0000
        cp = (uint16_t*)(MONO_BUF + KERNBASE);
        addr_6845 = MONO_BASE;
    }
    else
    {
        // 如果读出来了，有这块显存，即是彩显配置, 还原原来显存位置的值
        *cp = was;
        // 设置为彩显控制的 IO 地址，CGA_BASE: 0x3D4
        addr_6845 = CGA_BASE;
    }

    // Extract cursor location
    // 6845 索引寄存器的 index 0x0E（及十进制的14）== 光标位置(高位)
    // 6845 索引寄存器的 index 0x0F（及十进制的15）== 光标位置(低位)
    // 6845 reg 15 : Cursor Address (Low Byte)
    uint32_t pos;
    outb(addr_6845, 14);
    // 读出了光标位置(高位)
    pos = inb(addr_6845 + 1) << 8;
    outb(addr_6845, 15);
    // 读出了光标位置(低位)
    pos |= inb(addr_6845 + 1);

    // crt_buf 是 CGA 显存起始地址
    crt_buf = (uint16_t*) cp;
    // crt_pos 是 CGA 当前光标位置
    crt_pos = pos;
}

static bool serial_exists = 0;

static void serial_init(void)
{
    // Turn off the FIFO
    outb(COM1 + COM_FCR, 0);

    // Set speed; requires DLAB latch
    outb(COM1 + COM_LCR, COM_LCR_DLAB);
    outb(COM1 + COM_DLL, (uint8_t) (115200 / 9600));
    outb(COM1 + COM_DLM, 0);

    // 8 data bits, 1 stop bit, parity off; turn off DLAB latch
    outb(COM1 + COM_LCR, COM_LCR_WLEN8 & ~COM_LCR_DLAB);

    // No modem controls
    outb(COM1 + COM_MCR, 0);
    // Enable rcv interrupts
    outb(COM1 + COM_IER, COM_IER_RDI);

    // Clear any preexisting overrun indications and interrupts
    // Serial port doesn't exist if COM_LSR returns 0xFF
    serial_exists = (inb(COM1 + COM_LSR) != 0xFF);
    (void) inb(COM1 + COM_IIR);
    (void) inb(COM1 + COM_RX);
    
    if (serial_exists)
    {
        irq_setmask_8259A(irq_mask_8259A & ~(1 << IRQ_COM1));
    }
    
    ioapic_enable(IRQ_COM1, 0);
}

static void lpt_putc_sub(int c)
{
    int i;
    for (i = 0; !(inb(LPTPORT + 1) & 0x80) && i < 12800; i ++)
    {
        delay();
    }
    outb(LPTPORT + 0, c);
    outb(LPTPORT + 2, 0x08 | 0x04 | 0x01);
    outb(LPTPORT + 2, 0x08);
}

/* lpt_putc - copy console output to parallel port */
static void lpt_putc(int c)
{
    if (c != '\b')
    {
        lpt_putc_sub(c);
    }
    else
    {
        lpt_putc_sub('\b');
        lpt_putc_sub(' ');
        lpt_putc_sub('\b');
    }
}

/* cga_putc - print character to console */
// 输入字符串到屏幕实际就是把字符一个字节一个字节的放到显存地址空间 0xB8000 里
static void cga_putc(int c)
{
    // set black on white
    if (!(c & ~0xFF))
    {
        c |= 0x0700;
    }

    switch (c & 0xff)
    {
        case '\b':
            if (crt_pos > 0)
            {
                crt_pos --;
                crt_buf[crt_pos] = (c & ~0xff) | ' ';
            }
            break;
        case '\n':
            crt_pos += CRT_COLS;
        case '\r':
            crt_pos -= (crt_pos % CRT_COLS);
            break;
        default:
            crt_buf[crt_pos ++] = c;     // MMIO address, write the character
            break;
    }

    // What is the purpose of this?
    // 如果输出字符已经将整个屏幕占满，则往上滚动
    if (crt_pos >= CRT_SIZE)
    {
        int i;
        memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
        for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i ++)
        {
            crt_buf[i] = 0x0700 | ' ';
        }
        crt_pos -= CRT_COLS;
    }

    // move that little blinky thing
    outb(addr_6845, 14);
    outb(addr_6845 + 1, crt_pos >> 8);
    outb(addr_6845, 15);
    outb(addr_6845 + 1, crt_pos);
}

static void serial_putc_sub(int c)
{
    int i;
    for (i = 0; !(inb(COM1 + COM_LSR) & COM_LSR_TXRDY) && i < 12800; i ++)
    {
        delay();
    }
    outb(COM1 + COM_TX, c);
}

/* serial_putc - print character to serial port */
static void serial_putc(int c)
{
    if (c != '\b')
    {
        serial_putc_sub(c);
    }
    else
    {
        serial_putc_sub('\b');
        serial_putc_sub(' ');
        serial_putc_sub('\b');
    }
}

/* *
 * Here we manage the console input buffer, where we stash characters
 * received from the keyboard or serial port whenever the corresponding
 * interrupt occurs.
 * */

#define CONSBUFSIZE 512

// io 输入缓冲区
static struct
{
    uint8_t buf[CONSBUFSIZE];   // 输入缓冲区，串口或者键盘输入都缓存在这里
    uint32_t rpos;              // 指向当前可读的位置
    uint32_t wpos;              // 指向当前可写的位置
} cons;

/* *
 * cons_intr - called by device interrupt routines to feed input
 * characters into the circular console input buffer.
 * */
/*
 虽然从 io 发过来的数据，是通过中断通知 cpu 的，比 cpu 去主动轮询 io 要好
 但 cpu 响应中断之后，还是需要 cpu 循环读取 io 上的数据到内存，为了能完整一批批读完，需要先关闭中断
 但假如此时 io 速度很高，数据量很大，必然导致 cpu 花长时间在处理 io 中断响应，以及 io 数据读取上
 这样会大大降低 cpu 的效率，导致别的进程无法得到及时的调度执行，解决这个问题的方案要采用 DMA
*/
static void cons_intr(int (*proc)(void))
{
    int c;
    while ((c = (*proc)()) != -1)
    {
        if (c != 0)
        {
            cons.buf[cons.wpos ++] = c;
            // 如果已经写到末尾，又重头开始写，没有管 rpos 读没读完
            // 如果前面的没有读取完，会覆盖，所以实际这个缓存是一个环形缓存
            if (cons.wpos == CONSBUFSIZE)
            {
                cons.wpos = 0;
            }
        }
    }
}

/* serial_proc_data - get data from serial port */
/*
 端口地址参考 cat /proc/ioports
 3F8H　　　    1号串行口，发送/保持寄存器(RS232接口卡1)
 3F9H　　　    1号串行口，中断有效寄存器
 3FAH　　　    1号串行口，中断ID寄存器
 3FBH　　　    1号串行口，线控制寄存器
 3FCH　　　    1号串行口，调制解调控制寄存器
 3FDH　　　    1号串行口，线状态寄存器
 3FEH　　　    1号串行口，调制解调状态寄存器
 */
static int serial_proc_data(void)
{
    if (!(inb(COM1 + COM_LSR) & COM_LSR_DATA))
    {
        return -1;
    }
    int c = inb(COM1 + COM_RX);
    if (c == 127)
    {
        c = '\b';
    }
    return c;
}

/* serial_intr - try to feed input characters from serial port */
void serial_intr(void)
{
    if (serial_exists)
    {
        cons_intr(serial_proc_data);
    }
}

/***** Keyboard input code *****/

#define NO              0

#define SHIFT           (1<<0)
#define CTL             (1<<1)
#define ALT             (1<<2)

#define CAPSLOCK        (1<<3)
#define NUMLOCK         (1<<4)
#define SCROLLLOCK      (1<<5)

#define E0ESC           (1<<6)

static uint8_t shiftcode[256] = {
    [0x1D] = CTL,
    [0x2A] = SHIFT,
    [0x36] = SHIFT,
    [0x38] = ALT,
    [0x9D] = CTL,
    [0xB8] = ALT
};

static uint8_t togglecode[256] = {
    [0x3A] = CAPSLOCK,
    [0x45] = NUMLOCK,
    [0x46] = SCROLLLOCK
};

static uint8_t normalmap[256] = {
    NO,   0x1B, '1',  '2',  '3',  '4',  '5',  '6',  // 0x00
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  // 0x10
    'o',  'p',  '[',  ']',  '\n', NO,   'a',  's',
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  // 0x20
    '\'', '`',  NO,   '\\', 'z',  'x',  'c',  'v',
    'b',  'n',  'm',  ',',  '.',  '/',  NO,   '*',  // 0x30
    NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
    NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',  // 0x40
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
    '2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,   // 0x50
    [0xC7] = KEY_HOME,    [0x9C] = '\n' /*KP_Enter*/,
    [0xB5] = '/' /*KP_Div*/,  [0xC8] = KEY_UP,
    [0xC9] = KEY_PGUP,    [0xCB] = KEY_LF,
    [0xCD] = KEY_RT,      [0xCF] = KEY_END,
    [0xD0] = KEY_DN,      [0xD1] = KEY_PGDN,
    [0xD2] = KEY_INS,     [0xD3] = KEY_DEL
};

static uint8_t shiftmap[256] = {
    NO,   033,  '!',  '@',  '#',  '$',  '%',  '^',  // 0x00
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  // 0x10
    'O',  'P',  '{',  '}',  '\n', NO,   'A',  'S',
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  // 0x20
    '"',  '~',  NO,   '|',  'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  '<',  '>',  '?',  NO,   '*',  // 0x30
    NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
    NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',  // 0x40
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
    '2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,   // 0x50
    [0xC7] = KEY_HOME,    [0x9C] = '\n' /*KP_Enter*/,
    [0xB5] = '/' /*KP_Div*/,  [0xC8] = KEY_UP,
    [0xC9] = KEY_PGUP,    [0xCB] = KEY_LF,
    [0xCD] = KEY_RT,      [0xCF] = KEY_END,
    [0xD0] = KEY_DN,      [0xD1] = KEY_PGDN,
    [0xD2] = KEY_INS,     [0xD3] = KEY_DEL
};

#define C(x) (x - '@')

static uint8_t ctlmap[256] = {
    NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
    NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
    C('Q'),  C('W'),  C('E'),  C('R'),  C('T'),  C('Y'),  C('U'),  C('I'),
    C('O'),  C('P'),  NO,      NO,      '\r',    NO,      C('A'),  C('S'),
    C('D'),  C('F'),  C('G'),  C('H'),  C('J'),  C('K'),  C('L'),  NO,
    NO,      NO,      NO,      C('\\'), C('Z'),  C('X'),  C('C'),  C('V'),
    C('B'),  C('N'),  C('M'),  NO,      NO,      C('/'),  NO,      NO,
    [0x97] = KEY_HOME,
    [0xB5] = C('/'),      [0xC8] = KEY_UP,
    [0xC9] = KEY_PGUP,    [0xCB] = KEY_LF,
    [0xCD] = KEY_RT,      [0xCF] = KEY_END,
    [0xD0] = KEY_DN,      [0xD1] = KEY_PGDN,
    [0xD2] = KEY_INS,     [0xD3] = KEY_DEL
};

static uint8_t *charcode[4] = {
    normalmap,
    shiftmap,
    ctlmap,
    ctlmap
};

/* *
 * kbd_proc_data - get data from keyboard
 *
 * The kbd_proc_data() function gets data from the keyboard.
 * If we finish a character, return it, else 0. And return -1 if no data.
 * */
/*
 端口地址参考 cat /proc/ioports
 60H-61H　     键盘输入数据缓冲区
 61H　　　      AT:8042键盘控制寄存器/XT:8255输出寄存器
 62H　　　      8255输入寄存器
 63H　　　      8255命令方式寄存器
 64H　　　      8042键盘输入缓冲区/8042状态
 65H-6FH　     8255/8042专用
 */
static int kbd_proc_data(void)
{
    int c;
    uint8_t data;
    static uint32_t shift;

    if ((inb(KBSTATP) & KBS_DIB) == 0)
    {
        return -1;
    }

    data = inb(KBDATAP);

    if (data == 0xE0)
    {
        // E0 escape character
        shift |= E0ESC;
        return 0;
    }
    else if (data & 0x80)
    {
        // Key released
        data = (shift & E0ESC ? data : data & 0x7F);
        shift &= ~(shiftcode[data] | E0ESC);
        return 0;
    }
    else if (shift & E0ESC)
    {
        // Last character was an E0 escape; or with 0x80
        data |= 0x80;
        shift &= ~E0ESC;
    }

    shift |= shiftcode[data];
    shift ^= togglecode[data];

    c = charcode[shift & (CTL | SHIFT)][data];
    if (shift & CAPSLOCK)
    {
        if ('a' <= c && c <= 'z')
            c += 'A' - 'a';
        else if ('A' <= c && c <= 'Z')
            c += 'a' - 'A';
    }

    // Process special keys
    // Ctrl-Alt-Del: reboot
    if (!(~shift & (CTL | ALT)) && c == KEY_DEL)
    {
        cprintf("Rebooting!\n");
        outb(0x92, 0x3); // courtesy of Chris Frost
    }
    return c;
}

/* kbd_intr - try to feed input characters from keyboard */
static void kbd_intr(void)
{
    cons_intr(kbd_proc_data);
}

static void kbd_init(void)
{
    // drain the kbd buffer
    kbd_intr();
    irq_setmask_8259A(irq_mask_8259A & ~(1 << IRQ_KBD));
    ioapic_enable(IRQ_KBD, 0);
}

void cons_early_init(void)
{
    cga_init();
}

/* cons_init - initializes the console devices */
void cons_init(void)
{
    serial_init();
    kbd_init();
    if (!serial_exists)
    {
        cprintf("serial port does not exist!!\n");
    }
}

/* cons_putc - print a single character @c to console devices */
void cons_putc(int c)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        lpt_putc(c);
        cga_putc(c);
        serial_putc(c);
    }
    local_intr_restore(intr_flag);
}

/* *
 * cons_getc - return the next input character from console,
 * or 0 if none waiting.
 * */
/*
 要把外设的数据读入内存或把内存的数据传送到外设，一般都要通过 CPU 控制完成，
 如 CPU 程序查询或中断方式。利用中断进行数据传送，可以大大提高 CPU 的利用率。
 
 但是采用中断传送有它的缺点，对于一个高速 I/O 设备，以及批量交换数据的情况，
 只能采用 DMA 方式，才能解决效率和速度问题。
 DMA 在外设与内存间直接进行数据交换，而不通过 CPU，这样数据传送的速度就取决于存储器和外设的工作速度。
*/
int cons_getc(void)
{
    int c = 0;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        // poll for any pending input characters,
        // so that this function works even when interrupts are disabled
        // (e.g., when called from the kernel monitor).
        serial_intr();
        kbd_intr();

        // grab the next character from the input buffer.
        // 读和写的位置不同，说明还有读的内容，相同说明已经读完了
        // 这里只是判断内容是不是读完，如果是写得快读的慢，缓冲前面被覆盖，读取内容会丢失
        if (cons.rpos != cons.wpos)
        {
            c = cons.buf[cons.rpos++];
            if (cons.rpos == CONSBUFSIZE)
            {
                cons.rpos = 0;
            }
        }
    }
    local_intr_restore(intr_flag);
    return c;
}

