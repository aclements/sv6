import gdb
import gdb.printing
import re

class VectorPrinter(object):
    """Pretty-printer for vectors and static_vectors."""

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
        for i in range(int(self.val["size_"])):
            yield "[%d]" % i, items.dereference()
            items += 1

class IListPrinter(object):
    """Pretty-printer for ilists and islists."""

    def __init__(self, val):
        self.val = val
        self.type = val.type
        link_member = val.type.template_argument(1)
        self.link_type = link_member.type.target()
        self.link_off = int(link_member.cast(self.link_type.pointer()))

    def display_hint(self):
        return 'array'

    def to_string(self):
        return str(self.type)

    def children(self):
        link = self.val['head']
        i = 0
        while True:
            elt = link['next']
            # elt is a pointer value.  Compute the pointer to the link
            # field.
            link = gdb.Value(int(elt) + self.link_off).cast(self.link_type.pointer())
            if link['next'] == self.val['head']['next']:
                break
            yield "[%d]" % i, elt.dereference()
            i += 1

def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("xv6")
    pp.add_printer('vector', '^std::vector<.*>$', VectorPrinter)
    pp.add_printer('static_vector', '^static_vector<.*>$', VectorPrinter)
    pp.add_printer('ilist', '^ilist<.*>$', IListPrinter)
    pp.add_printer('islist', '^islist<.*>$', IListPrinter)
    return pp

gdb.printing.register_pretty_printer(
    gdb.current_objfile(),
    build_pretty_printer())

class PerCPU(gdb.Function):
    """Return the value of a static percpu variable.

    Usage: $percpu(var, [cpunum])

    If the CPU number is omitted, it defaults to the CPU of the
    selected thread.
    """

    def __init__(self):
        super(PerCPU, self).__init__('percpu')

    def invoke(self, basevar, cpu=None):
        if not (basevar.type.tag and
                basevar.type.tag.startswith('static_percpu<')):
            raise gdb.GdbError('Not a static_percpu')

        if cpu is None:
            cpu = gdb.selected_thread().num - 1
        else:
            cpu = int(cpu)

        # Get the key.  Unfortunately, G++ optimizes out the second
        # template argument, so we have to do this the dumb way.
        m = re.search(r'&([^ ,]+_key),', str(basevar.type))
        if not m:
            raise gdb.GdbError('Failed to parse type string %r' %
                               str(basevar.type))
        key = gdb.lookup_global_symbol(m.group(1))
        if key is None:
            raise gdb.GdbError('Failed to find per-cpu key %r' % m.group(1))

        # Compute the offset
        start = gdb.lookup_global_symbol('__percpu_start')
        offset = int(key.value().address) - int(start.value().address)

        # Get CPU's base
        cpubase = gdb.lookup_global_symbol('percpu_offsets').value()[cpu]

        # Put together new pointer
        return (cpubase + offset).cast(key.type.pointer()).dereference()

PerCPU()
