#!/usr/bin/env python3
"""
kmeta.py: takes in nm output on stdin, produces packed binary-searchable symbol list

The output takes the following format:

 - header (12 bytes):
    - number (4 bytes): # of entries
    - string offset (4 bytes): date
    - string offset (4 bytes): git description
 - search table (N x 12 bytes): one entry per symbol:
    - symbol address (8 bytes): address of the symbol represented here
    - string offset (4 bytes): offset of the string in the string table
 - string table (N x ? bytes): one entry per symbol
    - strings are simply placed into this table in sequence, null-terminated
    - strings do not need to be in the same order as the search table
    - strings are referenced by offset into this table
"""
import struct
import subprocess
import sys

symbol_file = sys.argv[1]
date_string = sys.argv[2]
git_string = sys.argv[3]

records = {}

for line in open(symbol_file):
    if not line.strip(): continue
    addr, kind, name = line.split()
    # kind is unused; we don't need it in the kernel
    addr = int(addr, 16)
    if not addr: continue  # don't include anything at address 0, because it'll lead to weird debug output
    if addr not in records or name < records[addr]:
        records[addr] = name

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

output = [struct.pack("<III", len(records), offsets[date_string], offsets[git_string])]
output += [struct.pack("<QI", address, offsets[name]) for address, name in zip(addresses, names)]
output += string_table

sys.stdout.buffer.write(b"".join(output))
