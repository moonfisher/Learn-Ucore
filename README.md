Ucore
======

Ucore 相关学习笔记都注释在代码里了<br>

Mac 或 Ubuntu 下编译 Ucore <br>
1）直接下载工程，终端执行 make clean;make; <br>
2）bin 目录下会生成内核镜像 ucore.img，交换分区 swap.img 和文件系统 sfs.img <br>

Mac 或 Ubuntu 下用 qemu 模拟 Ucore <br>
1）Mac 安装 qemu，终端执行 brew install qemu <br>
2）Ubuntu 安装 qemu，终端执行 sudo apt-get install qemu-kvmc <br>
3）运行 qemu，终端执行 qemu-system-i386 -S -s -parallel stdio -m 512M -drive file=bin/ucore.img -drive file=bin/swap.img -drive file=bin/sfs.img <br>

Mac 或 Ubuntu 下用 gdb 调试 Ucore <br>
1）终端下执行 gdb -q -tui -x tools/gdbinit <br>

