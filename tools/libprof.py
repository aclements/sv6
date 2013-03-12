import struct
import subprocess
import bisect
import collections

SAMP = struct.Struct("IIQ4QIIQ")

class SamplerFile(object):
    NTRACE = 4
    FLAGS, COUNT, RIP, TRACE0 = range(4)
    LATENCY, SOURCE, LOAD_ADDRESS = range(TRACE0+NTRACE, TRACE0+NTRACE+3)

    def __init__(self, fp):
        if isinstance(fp, basestring):
            fp = file(fp, "rb")
        self.__fp = fp

        # Read CPUs
        self.ncpu, = struct.unpack("Q", fp.read(8))
        self.__segments = []
        cpuinfo = struct.Struct("QQ")
        for cpu in range(self.ncpu):
            self.__segments.append(cpuinfo.unpack(fp.read(cpuinfo.size)))

    def read_cpu(self, cpu):
        if cpu < 0 or cpu >= self.ncpu:
            return []
        start, length = self.__segments[cpu]
        fp = self.__fp
        fp.seek(start)
        count = length / SAMP.size
        res = [None] * count
        for i in xrange(count):
            res[i] = SAMP.unpack(fp.read(SAMP.size))
        return res

class Symbols(object):
    def __init__(self, obj):
        self.__addrs = []
        self.__info = []
        for info in subprocess.check_output(["nm", "-Cn", obj]).splitlines():
            parts = info.split(None, 2)
            self.__addrs.append(int(parts[0], 16))
            self.__info.append((parts[1], parts[2]))

    def lookup(self, addr):
        i = bisect.bisect_right(self.__addrs, addr) - 1
        if i < 0 or i == len(self.__addrs) - 1:
            return Symbol(addr, None, None)
        return Symbol(addr, self.__info[i][1], self.__addrs[i])

class Symbol(collections.namedtuple("Symbol", "addr name base")):
    def __str__(self):
        if self.name:
            if self.base == self.addr:
                return self.name
            return "%s+%#x" % (self.name, self.addr - self.base)
        return "%#016x" % self.addr

class Addr2line(object):
    def __init__(self, obj):
        self.__p = subprocess.Popen(["addr2line", "-Cfsie", obj],
                                    stdin=subprocess.PIPE,
                                    stdout=subprocess.PIPE)
        self.__cache = {}

    def lookup(self, pc):
        if pc in self.__cache:
            return self.__cache[pc]

        print >> self.__p.stdin, "%#x" % pc
        # Add a dummy record so we can detect termination
        print >> self.__p.stdin

        frames = []
        self.__cache[pc] = frames
        while True:
            func = self.__p.stdout.readline().strip()
            source = self.__p.stdout.readline().strip()
            if len(frames) and func == "??":
                # Found dummy record
                break
            fname, line = source.split(":")
            line = int(line)
            frames.append(Frame(pc, func, fname, line))
            pc = 0
        return frames

class Frame(collections.namedtuple("Frame", "pc func fname line")):
    def __str__(self):
        if self.pc:
            pc = "%016x" % self.pc
        else:
            pc = "%-16s" % "(inlined by)"
        return "%s %s:%d %s" % (pc, self.fname, self.line, self.func)

class Histogram(object):
    def __init__(self):
        self.__counts = collections.Counter()
        self.min = self.max = self.mean = None
        self.samples = self.weight = 0

    def add(self, weight, count=1):
        self.__counts[weight] += count
        if self.min == None:
            self.min = self.max = weight
        elif weight < self.min:
            self.min = weight
        elif weight > self.max:
            self.max = weight
        self.samples += count
        self.weight += weight * count
        self.mean = self.weight / self.samples

    def to_line(self, width = 72, right = None, label = True):
        if self.min == None:
            return " " * width
        CHARS = map(unichr, range(0x2581, 0x2589))
        left = min(0, self.min)
        if right is None:
            right = max(0, self.max)
        if left == right:
            return "%s %s %s" % (self.min, CHARS[-1], self.max)
        leftLabel = (str(left) + " ") if label and left != 0 else ""
        rightLabel = (" " + str(right)) if label and right != 0 else ""
        width = max(width - len(leftLabel) - len(rightLabel), 1)
        bucket_width = float(right - left) / width
        buckets = [0] * width
        for weight, count in self.__counts.items():
            buckets[min(int((weight - left) / bucket_width),
                        width - 1)] += count
        maxbucket = max(buckets)
        res = []
        for b in buckets:
            if b == 0:
                res.append(u" ")
            else:
                res.append(CHARS[min(b * len(CHARS) / maxbucket, len(CHARS)-1)])
        return "%s%s%s" % (leftLabel, "".join(res), rightLabel)

class HistTree(object):
    def __init__(self, parent=None):
        self.__parent = parent
        self.__hists = {}
        self.__my_hist = Histogram()

    def add(self, path, weight, count=1):
        self.__my_hist.add(weight, count)
        if len(path):
            key = path[0]
            if key not in self.__hists:
                self.__hists[key] = HistTree(self)
            self.__hists[key].add(path[1:], weight, count)

    @property
    def hist(self):
        """Return the cumulative histogram of all children."""
        return self.__my_hist

    @property
    def parent(self):
        return self.__parent

    @property
    def fraction_of_parent(self):
        """Return the fraction of the parent's weight that belongs to
        the histogram tree rooted at self."""
        return float(self.hist.weight) / self.parent.hist.weight

    def __getitem__(self, key):
        return self.__hists[key]

    def items_sorted(self, reverse=False):
        """Return a list of (key, hist_tree) children sorted by the
        cumulative weight of hist_tree."""
        return sorted(self.__hists.items(),
                      key=lambda (k,ht): ht.hist.weight,
                      reverse=reverse)

# See Intel SDM Volume 3, table 18-13, and
# https://lkml.org/lkml/2013/1/24/302
LL_SOURCE_STR = [
    "unknown L3 miss",
    "L1 hit",
    "fill buffer hit",
    "L2 hit",
    "L3 hit, no snoop",
    "L3 hit, snoop clean",
    "L3 hit, snoop dirty",
    "reserved 0x7",
    "L3 miss, snoop hit",
    "reserved 0x9",
    "L3 miss, local DRAM, shared",
    "L3 miss, remote DRAM, shared",
    "L3 miss, local DRAM, exclusive",
    "L3 miss, remote DRAM, exclusive",
    "I/O memory",
    "un-cacheable memory"]

def ll_source_str(source):
    if source < 0 or source >= len(LL_SOURCE_STR):
        return "unknown source %#x" % source
    return LL_SOURCE_STR[source]

def self_less():
    import os, signal
    if not os.isatty(1):
        return
    r, w = os.pipe()
    if os.fork() > 0:
        os.dup2(r, 0)
        os.close(w)
        # Make sure less doesn't exit when we do
        signal.signal(signal.SIGCHLD, signal.SIG_IGN)
        try:
            os.execlp("less", "less", "-SFR")
        except:
            os.execlp("cat")
    os.close(r)
    os.dup2(w, 1)
    os.close(w)
    # Python ignores SIGPIPE by default, but we want to exit
    # immediately when less exits, like a good UNIX process
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    
