#!/usr/bin/env python3
"""
kmeta.py: takes in nm output on stdin, produces packed binary-searchable symbol list

The output takes the following format:

 - header (16 bytes):
    - magic number 0xb2499abb
    - branch table offset (4 bytes)
    - string table offset (4 bytes)
    - date string offset (4 bytes)
    - git description string offset (4 bytes)
 - symbol table (N x 12 bytes): one entry per symbol:
    - symbol address (8 bytes): address of the symbol represented here
    - string offset (4 bytes): offset of the string in the string table
 - branch table (M x 4 bytes): one entry per indirect branch:
    - bits 0..23: instruction address
    - bits 24..27: register number
    - bit 25: opcode (call=0, jmp=1)
 - string table: one entry per symbol
    - strings are simply placed into this table in sequence, null-terminated
    - strings do not need to be in the same order as the symbol table
    - strings are referenced by offset into this table
"""
import struct
import subprocess
import sys
import re

symbol_file = sys.argv[1]
objdump_file = sys.argv[2]
date_string = sys.argv[3]
git_string = sys.argv[4]

records = {}

for line in open(symbol_file):
    if not line.strip(): continue
    addr, kind, name = line.split()
    # kind is unused; we don't need it in the kernel
    addr = int(addr, 16)
    if not addr: continue  # don't include anything at address 0, because it'll lead to weird debug output
    if addr not in records or name < records[addr]:
        records[addr] = name

indirect_branches = []
registers = ["ax", "cx", "dx", "bx", "sp", "bp", "si", "di", "8", "9", "10", "11", "12", "13", "14", "15"]
register_indexes = { reg:i for (i, reg) in enumerate(registers) }
opcode_indexes = { "callq" : 0, "jmpq" : 1 }

# Match a jump or call to retpoline thunk. Then extract (1) the address, (2)
# whether it was a call or jump, and (3) which register it targeted.
indirect_branch_re = re.compile("([a-f0-9]*):.*(callq|jmpq).*<__x86_indirect_thunk_r(ax|cx|dx|bx|sp|bp|si|di|8|9|10|11|12|13|14|15)>.*")
for line in open(objdump_file):
    m = indirect_branch_re.match(line)
    if m is None: continue
    addr = int(m.group(1), 16) & 0xFFFFFF
    reg = register_indexes[m.group(3)]
    op = opcode_indexes[m.group(2)]
    indirect_branches.append((op << 28) | (reg << 24) | addr)

addresses, names = zip(*sorted(records.items()))
names = [exp for exp in subprocess.check_output(["c++filt", "--"] + list(names)).decode().split("\n") if exp]
names += [date_string, git_string]

offsets = {}
cur_offset = 0
string_table = []
for name in names:
    if name in offsets: continue
    enc_name = name.encode() + b'\0'
    string_table.append(enc_name)
    offsets[name] = cur_offset
    cur_offset += len(enc_name)

symbol_table_offset = 0
symbol_table_length = len(records) * 12
branch_table_offset = symbol_table_offset + symbol_table_length
branch_table_length = len(indirect_branches) * 4
string_table_offset = branch_table_offset + branch_table_length
string_table_length = len(string_table)

output = [struct.pack("<IIIII", 0xb2499abb, branch_table_offset, string_table_offset, \
                      offsets[date_string], offsets[git_string])]
output += [struct.pack("<QI", address, offsets[name]) for address, name in zip(addresses, names)]
output += [struct.pack("<I", b) for b in indirect_branches]
output += string_table

sys.stdout.buffer.write(b"".join(output))
