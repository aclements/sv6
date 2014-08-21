import gdb
import gdb.printing

class StaticVectorPrinter(object):
    """Pretty-printer for static_vectors."""

    def __init__(self, val):
        self.val = val
        self.type = val.type
        self.itype = val.type.template_argument(0)

    def display_hint(self):
        return 'array'

    def to_string(self):
        return "%s of length %d" % (self.type, self.val["size_"])

    def children(self):
        items = self.val["data_"].cast(self.itype.pointer())
        for i in range(self.val["size_"]):
            yield "[%d]" % i, items.dereference()
            items += 1

def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("xv6")
    pp.add_printer('static_vector', '^static_vector<.*>$', StaticVectorPrinter)
    return pp

gdb.printing.register_pretty_printer(
    gdb.current_objfile(),
    build_pretty_printer())

class PerCPU(gdb.Function):
    """Return the value of a static percpu variable.

    Usage: $percpu("varname", [cpunum])

    If the CPU number is omitted, it defaults to the CPU of the
    selected thread.
    """

    def __init__(self):
        super(PerCPU, self).__init__('percpu')

    def invoke(self, varname, cpu=None):
        varname = varname.string()
        if cpu is None:
            cpu = gdb.selected_thread().num - 1
        else:
            cpu = int(cpu)

        # Get the key
        key = gdb.lookup_global_symbol('__%s_key' % varname)
        if key is None:
            raise gdb.GdbError('No per-cpu symbol "%s"' % varname)

        # Compute the offset
        start = gdb.lookup_global_symbol('__percpu_start')
        offset = int(key.value().address) - int(start.value().address)

        # Get CPU's base
        cpubase = gdb.lookup_global_symbol('percpu_offsets').value()[cpu]

        # Put together new pointer
        return (cpubase + offset).cast(key.type.pointer()).dereference()

PerCPU()
