from optparse import OptionParser
import sys, re, json

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
    for n, syscall in enumerate(syscalls):
        syscall.num = n + 1

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

        print "u64 (*syscalls[])(u64, u64, u64, u64, u64, u64) = {"
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
            if num not in bynum:
                print '  nullptr,'
            else:
                print '  "%s",' % bynum[num].kname
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
  li a7, %(num)d
  ecall
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
