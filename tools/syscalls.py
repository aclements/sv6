from optparse import OptionParser
import sys, re, json

linux_syscall_numbers = {
    'read': 0,
    'write': 1,
    'open': 2,
    'close': 3,
    'stat': 4,
    'fstat': 5,
    'lstat': 6,
    'poll': 7,
    'lseek': 8,
    'mmap': 9,
    'mprotect': 10,
    'munmap': 11,
    'brk': 12,
    'sigaction': 13,
    'sigprocmask': 14,
    'sigreturn': 15,
    'ioctl': 16,
    'pread': 17,
    'pwrite': 18,
    'readv': 19,
    'writev': 20,
    'access': 21,
    'pipe': 22,
    'select': 23,
    'sched_yield': 24,
    'mremap': 25,
    'msync': 26,
    'mincore': 27,
    'madvise': 28,
    'shmget': 29,
    'shmat': 30,
    'shmctl': 31,
    'dup': 32,
    'dup2': 33,
    'pause': 34,
    'nanosleep': 35,
    'getitimer': 36,
    'alarm': 37,
    'setitimer': 38,
    'getpid': 39,
    'sendfile': 40,
    'socket': 41,
    'connect': 42,
    'accept': 43,
    'sendto': 44,
    'recvfrom': 45,
    'sendmsg': 46,
    'recvmsg': 47,
    'shutdown': 48,
    'bind': 49,
    'listen': 50,
    'getsockname': 51,
    'getpeername': 52,
    'socketpair': 53,
    'setsockopt': 54,
    'getsockopt': 55,
    'clone': 56,
    'fork': 57,
    'vfork': 58,
    'execve': 59,
    'exit': 60,
    'wait4': 61,
    'kill': 62,
    'uname': 63,
    'semget': 64,
    'semop': 65,
    'semctl': 66,
    'shmdt': 67,
    'msgget': 68,
    'msgsnd': 69,
    'msgrcv': 70,
    'msgctl': 71,
    'fcntl': 72,
    'flock': 73,
    'fsync': 74,
    'fdatasync': 75,
    'truncate': 76,
    'ftruncate': 77,
    'getdents': 78,
    'getcwd': 79,
    'chdir': 80,
    'fchdir': 81,
    'rename': 82,
    'mkdir': 83,
    'rmdir': 84,
    'creat': 85,
    'link': 86,
    'unlink': 87,
    'symlink': 88,
    'readlink': 89,
    'chmod': 90,
    'fchmod': 91,
    'chown': 92,
    'fchown': 93,
    'lchown': 94,
    'umask': 95,
    'gettimeofday': 96,
    'getrlimit': 97,
    'getrusage': 98,
    'sysinfo': 99,
    'times': 100,
    'ptrace': 101,
    'getuid': 102,
    'syslog': 103,
    'getgid': 104,
    'setuid': 105,
    'setgid': 106,
    'geteuid': 107,
    'getegid': 108,
    'setpgid': 109,
    'getppid': 110,
    'getpgrp': 111,
    'setsid': 112,
    'setreuid': 113,
    'setregid': 114,
    'getgroups': 115,
    'setgroups': 116,
    'setresuid': 117,
    'getresuid': 118,
    'setresgid': 119,
    'getresgid': 120,
    'getpgid': 121,
    'setfsuid': 122,
    'setfsgid': 123,
    'getsid': 124,
    'capget': 125,
    'capset': 126,
    'rt_sigpending': 127,
    'rt_sigtimedwait': 128,
    'rt_sigqueueinfo': 129,
    'rt_sigsuspend': 130,
    'sigaltstack': 131,
    'utime': 132,
    'mknod': 133,
    'uselib': 134,
    'personality': 135,
    'ustat': 136,
    'statfs': 137,
    'fstatfs': 138,
    'sysfs': 139,
    'getpriority': 140,
    'setpriority': 141,
    'sched_setparam': 142,
    'sched_getparam': 143,
    'sched_setscheduler': 144,
    'sched_getscheduler': 145,
    'sched_get_priority_max': 146,
    'sched_get_priority_min': 147,
    'sched_rr_get_interval': 148,
    'mlock': 149,
    'munlock': 150,
    'mlockall': 151,
    'munlockall': 152,
    'vhangup': 153,
    'modify_ldt': 154,
    'pivot_root': 155,
    '_sysctl': 156,
    'prctl': 157,
    'arch_prctl': 158,
    'adjtimex': 159,
    'setrlimit': 160,
    'chroot': 161,
    'sync': 162,
    'acct': 163,
    'settimeofday': 164,
    'mount': 165,
    'umount2': 166,
    'swapon': 167,
    'swapoff': 168,
    'reboot': 169,
    'sethostname': 170,
    'setdomainname': 171,
    'iopl': 172,
    'ioperm': 173,
    'create_module': 174,
    'init_module': 175,
    'delete_module': 176,
    'get_kernel_syms': 177,
    'query_module': 178,
    'quotactl': 179,
    'nfsservctl': 180,
    'getpmsg': 181,
    'putpmsg': 182,
    'afs_syscall': 183,
    'tuxcall': 184,
    'security': 185,
    'gettid': 186,
    'readahead': 187,
    'setxattr': 188,
    'lsetxattr': 189,
    'fsetxattr': 190,
    'getxattr': 191,
    'lgetxattr': 192,
    'fgetxattr': 193,
    'listxattr': 194,
    'llistxattr': 195,
    'flistxattr': 196,
    'removexattr': 197,
    'lremovexattr': 198,
    'fremovexattr': 199,
    'tkill': 200,
    'time': 201,
    'futex': 202,
    'sched_setaffinity': 203,
    'sched_getaffinity': 204,
    'set_thread_area': 205,
    'io_setup': 206,
    'io_destroy': 207,
    'io_getevents': 208,
    'io_submit': 209,
    'io_cancel': 210,
    'get_thread_area': 211,
    'lookup_dcookie': 212,
    'epoll_create': 213,
    'epoll_ctl_old': 214,
    'epoll_wait_old': 215,
    'remap_file_pages': 216,
    'getdents64': 217,
    'set_tid_address': 218,
    'restart_syscall': 219,
    'semtimedop': 220,
    'fadvise64': 221,
    'timer_create': 222,
    'timer_settime': 223,
    'timer_gettime': 224,
    'timer_getoverrun': 225,
    'timer_delete': 226,
    'clock_settime': 227,
    'clock_gettime': 228,
    'clock_getres': 229,
    'clock_nanosleep': 230,
    'exit_group': 231,
    'epoll_wait': 232,
    'epoll_ctl': 233,
    'tgkill': 234,
    'utimes': 235,
    'vserver': 236,
    'mbind': 237,
    'set_mempolicy': 238,
    'get_mempolicy': 239,
    'mq_open': 240,
    'mq_unlink': 241,
    'mq_timedsend': 242,
    'mq_timedreceive': 243,
    'mq_notify': 244,
    'mq_getsetattr': 245,
    'kexec_load': 246,
    'waitid': 247,
    'add_key': 248,
    'request_key': 249,
    'keyctl': 250,
    'ioprio_set': 251,
    'ioprio_get': 252,
    'inotify_init': 253,
    'inotify_add_watch': 254,
    'inotify_rm_watch': 255,
    'migrate_pages': 256,
    'openat': 257,
    'mkdirat': 258,
    'mknodat': 259,
    'fchownat': 260,
    'futimesat': 261,
    'newfstatat': 262,
    'unlinkat': 263,
    'renameat': 264,
    'linkat': 265,
    'symlinkat': 266,
    'readlinkat': 267,
    'fchmodat': 268,
    'faccessat': 269,
    'pselect6': 270,
    'ppoll': 271,
    'unshare': 272,
    'set_robust_list': 273,
    'get_robust_list': 274,
    'splice': 275,
    'tee': 276,
    'sync_file_range': 277,
    'vmsplice': 278,
    'move_pages': 279,
    'utimensat': 280,
    'epoll_pwait': 281,
    'signalfd': 282,
    'timerfd_create': 283,
    'eventfd': 284,
    'fallocate': 285,
    'timerfd_settime': 286,
    'timerfd_gettime': 287,
    'accept4': 288,
    'signalfd4': 289,
    'eventfd2': 290,
    'epoll_create1': 291,
    'dup3': 292,
    'pipe2': 293,
    'inotify_init1': 294,
    'preadv': 295,
    'pwritev': 296,
    'rt_tgsigqueueinfo': 297,
    'perf_event_open': 298,
    'recvmmsg': 299,
    'fanotify_init': 300,
    'fanotify_mark': 301,
    'prlimit64': 302,
    'name_to_handle_at': 303,
    'open_by_handle_at': 304,
    'clock_adjtime': 305,
    'syncfs': 306,
    'sendmmsg': 307,
    'setns': 308,
    'getcpu': 309,
    'process_vm_readv': 310,
    'process_vm_writev': 311,
    'kcmp': 312,
    'finit_module': 313,
    'sched_setattr': 314,
    'sched_getattr': 315,
    'renameat2': 316,
    'seccomp': 317,
    'getrandom': 318,
    'memfd_create': 319,
    'kexec_file_load': 320,
    'bpf': 321,
    'execveat': 322,
    'userfaultfd': 323,
    'membarrier': 324,
    'mlock2': 325,
    'copy_file_range': 326,
    'preadv2': 327,
    'pwritev2': 328,
    'pkey_mprotect': 329,
    'pkey_alloc': 330,
    'pkey_free': 331,
    'statx': 332,
    'io_pgetevents': 333,
    'rseq': 334,
    'pidfd_send_signal': 424,
    'io_uring_setup': 425,
    'io_uring_enter': 426,
    'io_uring_register': 427,
    'open_tree': 428,
    'move_mount': 429,
    'fsopen': 430,
    'fsconfig': 431,
    'fsmount': 432,
    'fspick': 433,
    'pidfd_open': 434,
    'clone3': 435,
}

linux_syscall_names = {num: name for name, num in linux_syscall_numbers.items()}

def main():
    parser = OptionParser(usage="usage: %prog [options] source...")
    parser.add_option("--kvectors", action="store_true",
                      help="output kernel syscall vectors")
    parser.add_option("--ustubs", action="store_true",
                      help="output user syscall stubs")
    parser.add_option("--udecls", action="store_true",
                      help="output user syscall declarations")
    (options, args) = parser.parse_args()

    if len(args) < 1:
        parser.print_help()
        parser.exit()

    # Parse source files
    syscalls = []
    for fname in args:
        syscalls.extend(parse(file(fname, "r")))

    # Generate syscall numbers
    n = 450
    for syscall in syscalls:
        syscall.num = linux_syscall_numbers.get(syscall.uname)
        if syscall.num is None:
            syscall.num = n
            n += 1

    # Output
    if options.kvectors:
        print "#include \"types.h\""
        print "#include \"kernel.hh\""
        print "#include <uk/unistd.h>"
        print "#include <uk/signal.h>"
        print
        for syscall in syscalls:
            print "extern %s %s(%s);" % (syscall.rettype, syscall.kname,
                                         ", ".join(syscall.kargs))
        print

        print "extern u64 (*const syscalls[])(u64, u64, u64, u64, u64, u64);"
        print "u64 (*const syscalls[])(u64, u64, u64, u64, u64, u64) = {"
        bynum = dict((s.num, s) for s in syscalls)
        for num in range(max(bynum.keys()) + 1):
            if num not in bynum:
                print "  nullptr,"
            else:
                print "  (u64(*)(u64,u64,u64,u64,u64,u64))%s," % bynum[num].kname
        print "};"
        print

        print 'const char* syscall_names[] = {'
        for num in range(max(bynum.keys()) + 1):
            syscallname = None
            if num in bynum:
                syscallname = bynum[num].kname
            elif num in linux_syscall_names:
                syscallname = linux_syscall_names[num]
            else:
                pass # raise Exception("could not find num: %d" % num)
            if syscallname is None:
                print '  nullptr,'
            else:
                print '  "%s",' % syscallname
        print '};'
        print

        print "extern const int nsyscalls = %d;" % (max(bynum.keys()) + 1)

    if options.ustubs:
        print "#include \"traps.h\""
        print
        for syscall in syscalls:
            print """\
.globl SYS_%(uname)s
SYS_%(uname)s = %(num)d

.globl %(uname)s
%(uname)s:
  movq $%(num)d, %%rax
  movq %%rcx, %%r10
  syscall
  ret
""" % syscall.__dict__

    if options.udecls:
        print "#include \"types.h\""
        print "#include <uk/unistd.h>"
        print
        print "BEGIN_DECLS"
        print
        types = set(typ for syscall in syscalls for typ in syscall.types())
        for typ in types:
            print typ + ";"
        for syscall in syscalls:
            extra = ""
            if syscall.flags.get("noret"):
                extra = " __attribute__((noreturn))"
            print "%s %s(%s)%s;" % (syscall.rettype, syscall.uname,
                                    ", ".join(syscall.uargs), extra)
        print
        print "END_DECLS"

class Syscall(object):
    def __init__(self, fp, kname, rettype, kargs, flags, num=None):
        self.kname, self.rettype, self.kargs, self.flags, self.num = \
            kname, rettype, kargs, flags, num

        self.basename = kname[4:]

        # Construct user space prototype
        self.uname = self.basename
        if "uargs" in flags:
            self.uargs = flags["uargs"]
        else:
            self.uargs = self.__make_uargs(kargs)

    @staticmethod
    def __make_uargs(kargs):
        uargs = []
        for karg in kargs:
            m = re.match("(.*?) *[a-z_]+$", karg)
            if karg.strip() == "void":
                atype = "void"
            elif m:
                atype = m.group(1)
            elif karg.strip() == "...":
                atype = "..."
            else:
                raise ParseError(fp.name, "could not parse args %r" % kargs)
            while True:
                atype2 = re.sub("userptr<(.*)>", r"\1*", atype)
                if atype2 == atype:
                    break
                atype = atype2
            if atype == "userptr_str":
                atype = "const char *"
            uargs.append(atype)
        return uargs

    def types(self):
        for uarg in self.uargs:
            m = re.search("(?:struct|union) +[a-zA-Z_][a-zA-Z0-9_]*", uarg)
            if m:
                yield m.group(0)

    def __repr__(self):
        return "Syscall(%r,%r,%r,%r,%r)" % (
            self.kname, self.rettype, self.kargs, self.flags, self.num)

class ParseError(RuntimeError):
    def __init__(self, fname, msg):
        RuntimeError.__init__(self, "%s: %s" % (fname, msg))

def parse(fp):
    res = []

    for flags, proto in re.findall(r"//SYSCALL(.*)([^{]*)", fp.read()):
        # Parse the prototype
        proto = " ".join(proto.split())
        m = re.match(r"(.+) ([a-z0-9_]+) *\(([^)]+)\)", proto)
        if not m:
            raise ParseError(fp.name, "could not parse prototype %r" % proto)
        rettype, name, kargs = m.groups()
        kargs = re.split(" *, *", kargs)

        # Parse the flags
        if flags.strip():
            flags = json.loads(flags)
        else:
            flags = {}

        res.append(Syscall(fp, name, rettype, kargs, flags))

    return res

if __name__ == "__main__":
    main()
