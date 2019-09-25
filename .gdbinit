set architecture i386:x86-64
target remote localhost:1234
symbol-file o.qemu/kernel.elf
#hb panic
#hb kerneltrap
hb trapasm.S:141

source tools/xv6-gdb.py
