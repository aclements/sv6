#!/usr/bin/env python2

import errno
import fcntl
import re
import os
import subprocess
import socket
import sys
import threading
import tempfile
import codexconfig

# verboseness options
WRITE_TO_STDOUT=True
LOG_REPLAY_RECORDS=False

XV6_KERNEL=os.path.dirname(os.path.realpath(__file__)) + '/../o.codex/kernel.elf'
QEMU=codexconfig.QEMU

# Don't enable network, so we can run multiple instances concurrently
#QEMU_ARGS="-smp 2 -m 512 -serial mon:stdio -nographic -numa node -numa node -net user -net nic,model=e1000 -redir tcp:2323::23 -redir tcp:8080::80" + " -kernel " + XV6_KERNEL
QEMU_ARGS="-smp 2 -m 512 -serial mon:stdio -nographic -numa node -numa node -kernel " + XV6_KERNEL

UPSTREAM_SOCK='/tmp/codexd.sock'
DOWNSTREAM_SOCK_NAME='codexd-relay.sock'

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
  def __init__(self, upstream_fp, downstream_accept_fp, exec_dir):
    '''takes ownership of fps'''
    self.__upstream_fp = upstream_fp
    self.__upstream_ps = PacketStream(upstream_fp.makefile("w"))
    self.__downstream_acccept_fp = downstream_accept_fp
    self.__exec_dir = exec_dir
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
      logfile = open(os.path.join(self.__exec_dir, "codexlog_%d" % (self.__ctr)), 'w')
      panicfile = os.path.join(self.__exec_dir, 'panic_%d.err' % (self.__ctr))
      replayfile = os.path.join(self.__exec_dir, 'replay_%d.trace' % (self.__ctr))
      recordfile = os.path.join(self.__exec_dir, 'record_%d.trace' % (self.__ctr))
      proc_env = dict(os.environ) # copy
      proc_env['CODEX_RELAY_SOCK']        = os.path.join(self.__exec_dir, DOWNSTREAM_SOCK_NAME)
      proc_env['CODEX_PANIC_FILE']        = panicfile
      proc_env['CODEX_REPLAY_SCHED_FILE'] = replayfile
      if LOG_REPLAY_RECORDS:
        proc_env['CODEX_RECORD_FILE'] = recordfile
      self.__ctr += 1
      proc = subprocess.Popen([QEMU] + QEMU_ARGS.split(),
          stdin=open('/dev/null', 'r'),
          stdout=subprocess.PIPE,
          stderr=subprocess.STDOUT,
          env=proc_env)

      fd = proc.stdout.fileno()
      fl = fcntl.fcntl(fd, fcntl.F_GETFL)
      fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)

      class TeeThread(threading.Thread):
        def __init__(self, poll_fd, out_fds):
          threading.Thread.__init__(self, name="tee-thd")
          self.poll_fd = poll_fd
          self.out_fds = out_fds
          self.running = True
          self.daemon = True

        def run_one_iter(self):
          while True:
            try:
              buf = os.read(self.poll_fd, 1024)
              # multi-cast
              for fd in self.out_fds:
                fd.write(buf)
              break
            except OSError as e:
              if e.errno != errno.EAGAIN:
                print >>sys.stderr, '[ERROR]: OS error %s' % str(e)
                sys.exit(1)

        def run(self):
          while self.running:
            self.run_one_iter()

      teethd = TeeThread(fd, [sys.stdout, logfile] if WRITE_TO_STDOUT else [logfile])
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
          logfile.flush()
          logfile.close()

          # done
          return True
    else:
      raise ProtocolError("unexpected response %r" % res)

def purge(d, pattern):
  for f in os.listdir(d):
    if re.search(pattern, f):
      os.remove(os.path.join(d, f))

def mkdir_p(path):
  try:
    os.makedirs(path)
  except OSError as exc: # Python >2.5
    if exc.errno == errno.EEXIST and os.path.isdir(path):
      pass
    else:
      raise

if __name__ == '__main__':
  if not os.path.exists(QEMU):
    print >>sys.stderr, \
        '[ERROR] Cannot find qemu (%s), did you forget to make install' % (QEMU)
    sys.exit(1)
  if not os.path.exists(XV6_KERNEL):
    print >>sys.stderr, \
        '[ERROR] Cannot find xv6 kernel (%s), did you forget to run make?' % (XV6_KERNEL)
    sys.exit(1)

  # get a new exection directory
  if 'CODEX_EXEC_DIR' in os.environ:
    exec_dir = os.environ['CODEX_EXEC_DIR']
    mkdir_p(exec_dir)
  else:
    exec_dir = tempfile.mkdtemp(prefix="codex")

  # create a new downstream socket
  s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
  s.bind(os.path.join(exec_dir, DOWNSTREAM_SOCK_NAME))
  s.listen(1)
  upstream_fp = socket.socket(socket.AF_UNIX)
  upstream_fp.connect(UPSTREAM_SOCK)
  client = Client(upstream_fp, s, exec_dir)
  while client.next():
    print "Finished a run"
    sys.stdout.flush()
