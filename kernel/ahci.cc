#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "pci.hh"
#include "pcireg.hh"
#include "disk.hh"
#include "ahcireg.hh"
#include "satareg.hh"
#include "idereg.hh"
#include "kstream.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "cpu.hh"

enum { fis_debug = 0 };

class ahci_hba;

struct ahci_port_page
{
  volatile struct ahci_recv_fis rfis __attribute__((aligned (256)));
  u8 pad[0x300];

  volatile struct ahci_cmd_header cmdh __attribute__((aligned (1024)));
  struct ahci_cmd_header cmdh_unused[31];

  volatile struct ahci_cmd_table cmdt __attribute__((aligned (128)));
};

class ahci_port : public disk
{
public:
  ahci_port(ahci_hba *h, int p, volatile ahci_reg_port* reg);

  void readv(kiovec *iov, int iov_cnt, u64 off) override;
  void writev(kiovec *iov, int iov_cnt, u64 off) override;
  void flush() override;
  void handle_port_irq();
  void handle_port_irq_locked();

  NEW_DELETE_OPS(ahci_port);

private:
  ahci_hba *const hba;
  const int pid;
  volatile ahci_reg_port *const preg;
  ahci_port_page *portpage;

  u64 fill_prd(void* addr, u64 nbytes);
  u64 fill_prd_v(kiovec* iov, int iov_cnt);
  void fill_fis(sata_fis_reg* fis);

  void dump();
  int wait();

  void issue(kiovec* iov, int iov_cnt, u64 off, int cmd);

  // For the disk read/write interface..
  spinlock io_lock;
  condvar io_cv;
  bool io_busy;
  bool io_done;

  class scoped_io : scoped_acquire {
  public:
    scoped_io(ahci_port* pp) : scoped_acquire(&pp->io_lock), p(pp) {
      while (p->io_busy)
        p->io_cv.sleep(&p->io_lock);
      p->io_busy = true;
      p->io_done = false;
    }

    ~scoped_io() {
      p->io_busy = false;
    }

  private:
    ahci_port* p;
  };

  void io_wait() {
    while (!io_done) {
      if (myproc()->get_state() == RUNNING) {
        io_cv.sleep(&io_lock);
      } else {
        handle_port_irq_locked();
      }
    }
  };
};

class ahci_hba : public irq_handler
{
public:
  ahci_hba(struct pci_func *pcif);
  ahci_hba(const ahci_hba &) = delete;
  ahci_hba &operator=(const ahci_hba &) = delete;

  static int attach(struct pci_func *pcif);

  void handle_irq() override;

  NEW_DELETE_OPS(ahci_hba);

private:
  const u32 membase;
  volatile ahci_reg *const reg;
  ahci_port* port[32];
};

void
initahci(void)
{
  pci_register_class_driver(PCI_CLASS_MASS_STORAGE,
                            PCI_SUBCLASS_MASS_STORAGE_SATA,
                            &ahci_hba::attach);
}

int
ahci_hba::attach(struct pci_func *pcif)
{
  if (PCI_INTERFACE(pcif->dev_class) != 0x01) {
    console.println("AHCI: not an AHCI controller");
    return 0;
  }

  console.println("AHCI: attaching");
  pci_func_enable(pcif);
  ahci_hba *hba __attribute__((unused)) = new ahci_hba(pcif);
  console.println("AHCI: done");
  return 1;
}

ahci_hba::ahci_hba(struct pci_func *pcif)
  : membase(pcif->reg_base[5]),
    reg((ahci_reg*) p2v(membase))
{
  reg->g.ghc |= AHCI_GHC_AE;

  for (int i = 0; i < 32; i++) {
    if (reg->g.pi & (1 << i)) {
      port[i] = new ahci_port(this, i, &reg->port[i].p);
    }
  }

  irq ahci_irq = extpic->map_pci_irq(pcif);
  ahci_irq.enable();
  ahci_irq.register_handler(this);
  reg->g.ghc |= AHCI_GHC_IE;
}

void
ahci_hba::handle_irq()
{
  for (int i = 0; i < 32; i++) {
    if (!(reg->g.is & (1 << i)))
      continue;

    if (port[i]) {
      port[i]->handle_port_irq();
    } else {
      cprintf("AHCI: stray irq for port %d, clearing\n", i);
    }

    /* AHCI 1.3, section 10.7.2.1 says we need to first clear the
     * port interrupt status and then clear the host interrupt
     * status.  It's fine to do this even after we've processed the
     * port interrupt: if any port interrupts happened in the mean
     * time, the host interrupt bit will just get set again. */
    reg->g.is = (1 << i);
  }
}


ahci_port::ahci_port(ahci_hba *h, int p, volatile ahci_reg_port* reg)
  : hba(h), pid(p), preg(reg), io_busy(false)
{
  portpage = (ahci_port_page*) kalloc("ahci_port_page");
  assert(portpage);

  /* Wait for port to quiesce */
  if (preg->cmd & (AHCI_PORT_CMD_ST | AHCI_PORT_CMD_CR |
                   AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_FR)) {
    cprintf("AHCI: port %d active, clearing..\n", pid);
    preg->cmd &= ~(AHCI_PORT_CMD_ST | AHCI_PORT_CMD_FRE);
    microdelay(500 * 1000);

    if (preg->cmd & (AHCI_PORT_CMD_CR | AHCI_PORT_CMD_FR)) {
      cprintf("AHCI: port %d still active, giving up\n", pid);
      return;
    }
  }

  /* Initialize memory buffers */
  portpage->cmdh.ctba = v2p((void*) &portpage->cmdt);
  preg->clb = v2p((void*) &portpage->cmdh);
  preg->fb = v2p((void*) &portpage->rfis);
  preg->ci = 0;

  /* Clear any errors first, otherwise the chip wedges */
  preg->serr = ~0;
  preg->serr = 0;

  /* Enable receiving frames */
  preg->cmd |= AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_ST |
               AHCI_PORT_CMD_SUD | AHCI_PORT_CMD_POD |
               AHCI_PORT_CMD_ACTIVE;

  /* Check if there's anything there */
  u32 phystat = preg->ssts;
  if (!phystat) {
    cprintf("AHCI: port %d: not connected\n", pid);
    return;
  }

  /* Try to send an IDENTIFY */
  union {
    struct identify_device id;
    char buf[512];
  } id_buf;

  struct sata_fis_reg fis;
  memset(&fis, 0, sizeof(fis));
  fis.type = SATA_FIS_TYPE_REG_H2D;
  fis.cflag = SATA_FIS_REG_CFLAG;
  fis.command = IDE_CMD_IDENTIFY;
  fis.sector_count = 1;

  fill_prd(&id_buf, sizeof(id_buf));
  fill_fis(&fis);
  preg->ci |= 1;

  if (wait() < 0) {
    cprintf("AHCI: port %d: cannot identify\n", pid);
    return;
  }

  if (!(id_buf.id.features86 & IDE_FEATURE86_LBA48)) {
    cprintf("AHCI: disk too small, driver requires LBA48\n");
    return;
  }

  u64 sectors = id_buf.id.lba48_sectors;
  dk_nbytes = sectors * 512;
  memcpy(dk_model, id_buf.id.model, sizeof(id_buf.id.model));
  dk_model[sizeof(dk_model) - 1] = '\0';
  memcpy(dk_serial, id_buf.id.serial, sizeof(id_buf.id.serial));
  dk_serial[sizeof(dk_serial) - 1] = '\0';
  memcpy(dk_firmware, id_buf.id.firmware, sizeof(id_buf.id.firmware));
  dk_firmware[sizeof(dk_firmware) - 1] = '\0';
  snprintf(dk_busloc, sizeof(dk_busloc), "ahci.%d", pid);

  /* Enable write-caching, read look-ahead */
  memset(&fis, 0, sizeof(fis));
  fis.type = SATA_FIS_TYPE_REG_H2D;
  fis.cflag = SATA_FIS_REG_CFLAG;
  fis.command = IDE_CMD_SETFEATURES;
  fis.features = IDE_FEATURE_WCACHE_ENA;

  fill_prd(0, 0);
  fill_fis(&fis);
  preg->ci |= 1;

  if (wait() < 0) {
    cprintf("AHCI: port %d: cannot enable write caching\n", pid);
    return;
  }

  fis.features = IDE_FEATURE_RLA_ENA;
  fill_fis(&fis);
  preg->ci |= 1;

  if (wait() < 0) {
    cprintf("AHCI: port %d: cannot enable read lookahead\n", pid);
    return;
  }

  /* Enable interrupts */
  preg->ie = AHCI_PORT_INTR_DHRE;

  disk_register(this);
}

u64
ahci_port::fill_prd_v(kiovec* iov, int iov_cnt)
{
  u64 nbytes = 0;

  volatile ahci_cmd_table *cmd = (ahci_cmd_table *) &portpage->cmdt;
  assert(iov_cnt < sizeof(cmd->prdt) / sizeof(cmd->prdt[0]));

  for (int slot = 0; slot < iov_cnt; slot++) {
    cmd->prdt[slot].dba = v2p(iov[slot].iov_base);
    cmd->prdt[slot].dbc = iov[slot].iov_len - 1;
    nbytes += iov[slot].iov_len;
  }

  portpage->cmdh.prdtl = iov_cnt;
  return nbytes;
}

u64
ahci_port::fill_prd(void* addr, u64 nbytes)
{
  kiovec iov = { addr, nbytes };
  return fill_prd_v(&iov, 1);
}

static void
print_fis(sata_fis_reg *r)
{
  cprintf("SATA FIS Reg\n");
  cprintf("type:              0x%x\n", r->type);
  cprintf("cflag:             0x%x\n", r->cflag);
  cprintf("command/status:    0x%x\n", r->command);
  cprintf("features/error:    0x%x\n", r->features);
  cprintf("lba_0:             0x%x\n", r->lba_0);
  cprintf("lba_1:             0x%x\n", r->lba_1);
  cprintf("lba_2:             0x%x\n", r->lba_2);
  cprintf("dev_head:          0x%x\n", r->dev_head);
  cprintf("lba_3:             0x%x\n", r->lba_3);
  cprintf("lba_4:             0x%x\n", r->lba_4);
  cprintf("lba_5:             0x%x\n", r->lba_5);
  cprintf("features_ex:       0x%x\n", r->features_ex);
  cprintf("sector_count:      0x%x\n", r->sector_count);
  cprintf("sector_count_ex:   0x%x\n", r->sector_count_ex);
  cprintf("control:           0x%x\n", r->control);
}

void
ahci_port::fill_fis(sata_fis_reg* fis)
{
  memcpy((void*) &portpage->cmdt.cfis[0], fis, sizeof(*fis));
  portpage->cmdh.flags = sizeof(*fis) / sizeof(u32);
  if (fis_debug)
    print_fis(fis);
}

void
ahci_port::dump()
{
  cprintf("AHCI port %d dump:\n", pid);
  cprintf("PxCMD    = 0x%x\n", preg->cmd);
  cprintf("PxTFD    = 0x%x\n", preg->tfd);
  cprintf("PxSIG    = 0x%x\n", preg->sig);
  cprintf("PxCI     = 0x%x\n", preg->ci);
  cprintf("SStatus  = 0x%x\n", preg->ssts);
  cprintf("SControl = 0x%x\n", preg->sctl);
  cprintf("SError   = 0x%x\n", preg->serr);
  // cprintf("GHC      = 0x%x\n", hba->reg->ghc);
}

int
ahci_port::wait()
{
  u64 ts_start = rdtsc();

  for (;;) {
    u32 tfd = preg->tfd;
    u8 stat = AHCI_PORT_TFD_STAT(tfd);
    if (!(stat & IDE_STAT_BSY) && !(preg->ci & 1))
      return 0;

    u64 ts_diff = rdtsc() - ts_start;
    if (ts_diff > 1000 * 1000 * 1000) {
      cprintf("ahci_port::wait: stuck for %lx cycles\n", ts_diff);
      dump();
      return -1;
    }
  }
}

void
ahci_port::handle_port_irq()
{
  scoped_acquire x(&io_lock);

  preg->is = ~0;
  handle_port_irq_locked();
}

void
ahci_port::handle_port_irq_locked()
{
  if (io_busy && !io_done && !(preg->ci & 1)) {
    io_done = true;
    io_cv.wake_all();

    u32 tfd = preg->tfd;
    if (AHCI_PORT_TFD_STAT(tfd) & (IDE_STAT_ERR | IDE_STAT_DF)) {
      cprintf("AHCI: port %d: status %02x, err %02x\n",
              pid, AHCI_PORT_TFD_STAT(tfd), AHCI_PORT_TFD_ERR(tfd));
    }
  }
}

void
ahci_port::readv(kiovec* iov, int iov_cnt, u64 off)
{
  scoped_io x(this);
  issue(iov, iov_cnt, off, IDE_CMD_READ_DMA_EXT);
  io_wait();
}

void
ahci_port::writev(kiovec* iov, int iov_cnt, u64 off)
{
  scoped_io x(this);
  issue(iov, iov_cnt, off, IDE_CMD_WRITE_DMA_EXT);
  io_wait();
}

void
ahci_port::flush()
{
  scoped_io x(this);
  issue(nullptr, 0, 0, IDE_CMD_FLUSH_CACHE);
  io_wait();
}

void
ahci_port::issue(kiovec* iov, int iov_cnt, u64 off, int cmd)
{
  assert((off % 512) == 0);

  sata_fis_reg fis;
  memset(&fis, 0, sizeof(fis));
  fis.type = SATA_FIS_TYPE_REG_H2D;
  fis.cflag = SATA_FIS_REG_CFLAG;
  fis.command = cmd;

  u64 len = fill_prd_v(iov, iov_cnt);
  assert((len % 512) == 0);
  assert(len <= DISK_REQMAX);

  if (len) {
    u64 sector_off = off / 512;

    fis.dev_head = IDE_DEV_LBA;
    fis.control = IDE_CTL_LBA48;

    fis.sector_count = len / 512;
    fis.lba_0 = (sector_off >>  0) & 0xff;
    fis.lba_1 = (sector_off >>  8) & 0xff;
    fis.lba_2 = (sector_off >> 16) & 0xff;
    fis.lba_3 = (sector_off >> 24) & 0xff;
    fis.lba_4 = (sector_off >> 32) & 0xff;
    fis.lba_5 = (sector_off >> 40) & 0xff;

    if (cmd == IDE_CMD_READ_DMA_EXT) {
      portpage->cmdh.prdbc = 0;
    }

    if (cmd == IDE_CMD_WRITE_DMA_EXT) {
      portpage->cmdh.flags |= AHCI_CMD_FLAGS_WRITE;
      portpage->cmdh.prdbc = len;
    }
  }

  fill_fis(&fis);
  preg->ci |= 1;
}
