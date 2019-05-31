#include "ulib.h"
#include "x86.h"

int main(int argc, char *argv[])
{
    /*write  io port 0x8900 "Shutdown" to shutdown qemu
    * NOTICE: only used in modified qemu (with 0x8900 IO write support)
    *         in qemu/hw/pc.c::bochs_bios_write function
    **/
    char *p = "Shutdown";
    for( ; *p; p++)
        outb(0x8900, *p);
    return 0;
}
