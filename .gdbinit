set architecture i386:x86-64
target remote localhost:1234
symbol-file o.qemu/kernel.elf
br panic
br kerneltrap

python execfile("tools/xv6-gdb.py")
