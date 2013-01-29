#!/usr/bin/env python2

import os
import subprocess
import socket
import sys

QEMU=os.path.expanduser('~/qemu-bin/bin/qemu-system-x86_64')
XV6_KERNEL=os.path.expanduser('~/xv6/o.qemu/kernel.elf')
QEMU_ARGS="-smp 2 -m 512 -serial mon:stdio -nographic -numa node -numa node -net user -net nic,model=e1000 -redir tcp:2323::23 -redir tcp:8080::80" + " -kernel " + XV6_KERNEL

UPSTREAM_SOCK='/tmp/codexd.sock'
DOWNSTREAM_SOCK='/tmp/codexd-relay.sock'

class ProtocolError(RuntimeError):
  pass

class PacketStream(object):
  FLUSH = object()

  def __init__(self, fp):
    self.__fp = fp

  def read(self):
    # XXX(stephentu): usage of read() is broken-
    # no reason read has to return exactly the amount of
    # bytes requested

    hdr = self.__fp.read(4)

    #if len(hdr) == 0:
    #  return None
    #elif len(hdr) != 4:
    #  raise ProtocolError("unexpected EOF")

    if len(hdr) != 4:
      raise ProtocolError("unexpected EOF")
    try:
      l = int(hdr, 16)
    except:
      raise ProtocolError("read bad packet length")

    if l == 0:
      payload = PacketStream.FLUSH
    elif l < 4:
      raise ProtocolError("read bad packet length")
    else:
      payload = self.__fp.read(l - 4)
      if len(payload) != l - 4:
        raise ProtocolError("unexpected EOF")

    lf = self.__fp.read(1)
    if lf == "":
      raise ProtocolError("unexpected EOF")
    elif lf != "\n":
      raise ProtocolError("read bad packet terminator")
    return payload

  def write(self, payload):
    if payload == PacketStream.FLUSH:
      self.__fp.write("0000\n")
    else:
      if len(payload) + 4 > 0xffff:
        raise RuntimeError("packet too large to send")
      self.__fp.write("%04x" % (len(payload) + 4))
      self.__fp.write(payload)
      self.__fp.write("\n")
      return self

  def flush(self):
    self.__fp.flush()

  def close(self):
    self.__fp.close()

class Client(object):
  def __init__(self, upstream_fp, downstream_accept_fp):
    '''takes ownership of fps'''
    self.__upstream_fp = upstream_fp
    self.__upstream_ps = PacketStream(upstream_fp.makefile("w"))
    self.__downstream_acccept_fp = downstream_accept_fp

  def close(self):
    self.__upstream_ps.close()
    self.__upstream_fp.close()
    self.__downstream_acccept_fp.close()

  def next(self):
    self.__upstream_ps.write("next").flush()
    res = self.__upstream_ps.read()
    if res == "done":
      return False
    elif res == "reset":
      # spawn downstream proc
      proc = subprocess.Popen([QEMU] + QEMU_ARGS.split(),
          stdin=open('/dev/null', 'r'))
      downstream_fp, _ = s.accept()
      downstream_ps = PacketStream(downstream_fp.makefile("w"))
      while True:
        sumbuf = self.__upstream_ps.read()
        downstream_ps.write(sumbuf)
        if sumbuf == PacketStream.FLUSH:
          # flush downstream
          downstream_ps.flush()

          # start polling downstream
          self.__upstream_ps.write("extend")
          while True:
            res = downstream_ps.read()
            self.__upstream_ps.write(res)
            if res == PacketStream.FLUSH:
              self.__upstream_ps.flush()
              break

          # cleanup
          downstream_ps.close()
          downstream_fp.close()
          proc.kill()
          proc.wait()

          # done
          return True
    else:
      raise ProtocolError("unexpected response %r" % res)

if __name__ == '__main__':
  if not os.path.exists(QEMU):
    print >>sys.stderr, \
        '[ERROR] Cannot find qemu (%s), did you forget to make install' % (QEMU)
  if not os.path.exists(XV6_KERNEL):
    print >>sys.stderr, \
        '[ERROR] Cannot find xv6 kernel (%s), did you forget to run make?' % (XV6_KERNEL)

  try:
    os.remove(DOWNSTREAM_SOCK)
  except OSError as e:
    if e.errno != 2:
      raise e

  s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
  s.bind(DOWNSTREAM_SOCK)
  s.listen(1)
  upstream_fp = socket.socket(socket.AF_UNIX)
  upstream_fp.connect(UPSTREAM_SOCK)
  client = Client(upstream_fp, s)
  while client.next():
    print "Finished a run"
    sys.stdout.flush()
