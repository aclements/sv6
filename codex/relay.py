#!/usr/bin/env python2

import errno
import fcntl
import re
import os
import subprocess
import socket
import sys
import threading

QEMU=os.path.expanduser('~/qemu-bin/bin/qemu-system-x86_64')
XV6_KERNEL=os.path.expanduser('~/xv6/o.qemu/kernel.elf')
QEMU_ARGS="-smp 2 -m 512 -serial mon:stdio -nographic -numa node -numa node -net user -net nic,model=e1000 -redir tcp:2323::23 -redir tcp:8080::80" + " -kernel " + XV6_KERNEL

UPSTREAM_SOCK='/tmp/codexd.sock'
DOWNSTREAM_SOCK='/tmp/codexd-relay.sock'
LOGFILE='/tmp/codexlog'
LOGFILEPAT=(LOGFILE + r'\d+')

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
    self.__ctr = 0

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
      stdout = open(LOGFILE + str(self.__ctr), 'w')
      self.__ctr += 1
      proc = subprocess.Popen([QEMU] + QEMU_ARGS.split(),
          stdin=open('/dev/null', 'r'),
          stdout=subprocess.PIPE,
          stderr=subprocess.STDOUT)

      fd = proc.stdout.fileno()
      fl = fcntl.fcntl(fd, fcntl.F_GETFL)
      fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)

      class TeeThread(threading.Thread):
        def __init__(self, poll_fd, out):
          threading.Thread.__init__(self, name="tee-thd")
          self.poll_fd = poll_fd
          self.out = out
          self.running = True

        def run_one_iter(self):
          while True:
            try:
              buf = os.read(self.poll_fd, 1024)
              sys.stdout.write(buf)
              self.out.write(buf)
              break
            except OSError as e:
              if e.errno != errno.EAGAIN:
                print >>sys.stderr, '[ERROR]: OS error %s' % str(e)
                sys.exit(1)

        def run(self):
          while self.running:
            self.run_one_iter()

      teethd = TeeThread(fd, stdout)
      teethd.start()

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

          teethd.running = False
          teethd.join()
          teethd.run_one_iter() # for good measure

          proc.kill()
          proc.wait()
          stdout.flush()
          stdout.close()

          # done
          return True
    else:
      raise ProtocolError("unexpected response %r" % res)

def purge(d, pattern):
  for f in os.listdir(d):
    if re.search(pattern, f):
      os.remove(os.path.join(d, f))

if __name__ == '__main__':
  if not os.path.exists(QEMU):
    print >>sys.stderr, \
        '[ERROR] Cannot find qemu (%s), did you forget to make install' % (QEMU)
    sys.exit(1)
  if not os.path.exists(XV6_KERNEL):
    print >>sys.stderr, \
        '[ERROR] Cannot find xv6 kernel (%s), did you forget to run make?' % (XV6_KERNEL)
    sys.exit(1)

  try:
    os.remove(DOWNSTREAM_SOCK)
  except OSError as e:
    if e.errno != 2:
      raise e

  purge('/tmp', LOGFILEPAT)

  s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
  s.bind(DOWNSTREAM_SOCK)
  s.listen(1)
  upstream_fp = socket.socket(socket.AF_UNIX)
  upstream_fp.connect(UPSTREAM_SOCK)
  client = Client(upstream_fp, s)
  while client.next():
    print "Finished a run"
    sys.stdout.flush()
