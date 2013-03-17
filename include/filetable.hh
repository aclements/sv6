#include <atomic>
#include "percpu.hh"
#include "ref.hh"

class filetable : public referenced {
private:
  static const int cpushift = 16;
  static const int fdmask = (1 << cpushift) - 1;

public:
  static sref<filetable> alloc() {
    return sref<filetable>::transfer(new filetable());
  }

  sref<filetable> copy() {
    filetable* t = new filetable(false);

    for(int cpu = 0; cpu < NCPU; cpu++) {
      for(int fd = 0; fd < NOFILE; fd++) {
        file *f = ofile_[cpu][fd];
        if (f) {
          f->inc();
          t->ofile_[cpu][fd].store(f, std::memory_order_relaxed);
        } else {
          t->ofile_[cpu][fd].store(nullptr, std::memory_order_relaxed);
        }
      }
    }
    std::atomic_thread_fence(std::memory_order_release);
    return sref<filetable>::transfer(t);
  }
  
  bool getfile(int fd, sref<file> *sf) {
    int cpu = fd >> cpushift;
    fd = fd & fdmask;

    if (cpu < 0 || cpu >= NCPU)
      return false;

    if (fd < 0 || fd >= NOFILE)
      return false;

    file* f = ofile_[cpu][fd];
    if (!f || !sf->init_nonzero(f))
      return false;
    return true;
  }

  int allocfd(struct file *f, bool percpu = false) {
    int cpu = percpu ? myid() : 0;
    for (int fd = 0; fd < NOFILE; fd++)
      if (ofile_[cpu][fd] == nullptr && cmpxch(&ofile_[cpu][fd], (file*)nullptr, f))
        return (cpu << cpushift) | fd;
    cprintf("filetable::allocfd: failed\n");
    return -1;
  }

  void close(int fd) {
    // XXX(sbw) if f->ref_ > 1 the kernel will not actually close 
    // the file when this function returns (i.e. sys_close can return 
    // while the file/pipe/socket is still open).
    int cpu = fd >> cpushift;
    fd = fd & fdmask;

    if (cpu < 0 || cpu >= NCPU) {
      cprintf("filetable::close: bad fd cpu %u\n", cpu);
      return;
    }

    if (fd < 0 || fd >= NOFILE) {
      cprintf("filetable::close: bad fd %u\n", fd);
      return;
    }

    file* f = ofile_[cpu][fd].exchange(nullptr);
    if (f != nullptr)
      f->dec();
    else
      cprintf("filetable::close: bad fd %u\n", fd);
  }

  bool replace(int fd, struct file* newf) {
    int cpu = fd >> cpushift;
    fd = fd & fdmask;

    if (cpu < 0 || cpu >= NCPU) {
      cprintf("filetable::replace: bad fd cpu %u\n", cpu);
      return false;
    }

    if (fd < 0 || fd >= NOFILE) {
      cprintf("filetable::replace: bad fd %u\n", fd);
      return false;
    }

    file* oldf = ofile_[cpu][fd].exchange(newf);
    if (oldf != nullptr)
      oldf->dec();
    return true;
  }

private:
  filetable(bool clear = true) {
    if (!clear)
      return;
    for(int cpu = 0; cpu < NCPU; cpu++)
      for(int fd = 0; fd < NOFILE; fd++)
        ofile_[cpu][fd].store(nullptr, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
  }

  ~filetable() {
    for(int cpu = 0; cpu < NCPU; cpu++){
      for(int fd = 0; fd < NOFILE; fd++){
        if (ofile_[cpu][fd].load() != nullptr) {
          ofile_[cpu][fd].load()->dec();
          ofile_[cpu][fd] = nullptr;
        }
      }
    }
  }

  filetable& operator=(const filetable&);
  filetable(const filetable& x);
  NEW_DELETE_OPS(filetable);  

  percpu<std::atomic<file*>[NOFILE]> ofile_;
};
