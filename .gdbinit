set architecture i386:x86-64
target remote localhost:1234
symbol-file o.qemu/kernel.elf
br panic
br kerneltrap

python exec(compile(open("tools/xv6-gdb.py").read(), "tools/xv6-gdb.py", "exec"))
