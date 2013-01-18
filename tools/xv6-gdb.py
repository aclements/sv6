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
        for i in xrange(self.val["size_"]):
            yield "[%d]" % i, items.dereference()
            items += 1

def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("xv6")
    pp.add_printer('static_vector', '^static_vector<.*>$', StaticVectorPrinter)
    return pp

gdb.printing.register_pretty_printer(
    gdb.current_objfile(),
    build_pretty_printer())
