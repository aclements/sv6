#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "pci.hh"
#include "pcireg.hh"
#include "spinlock.h"
#include "apic.hh"
#include "irq.hh"
#include "e1000reg.hh"
#include "kstream.hh"
#include "netdev.hh"

#define TX_RING_SIZE 64
#define RX_RING_SIZE 64

struct e1000_model;

class e1000 : public netdev, irq_handler
{
  const struct e1000_model * const model_;
  const u32 membase_;
  const u32 iobase_;

  volatile u32 txclean_;
  volatile u32 txinuse_;

  volatile u32 rxclean_;
  volatile u32 rxuse_;

  u8 hwaddr_[6];

  struct wiseman_txdesc txd_[TX_RING_SIZE] __attribute__((aligned (16)));
  struct wiseman_rxdesc rxd_[RX_RING_SIZE] __attribute__((aligned (16)));

  struct spinlock lk_;

  bool valid_;

  NEW_DELETE_OPS(e1000);

  u32 erd(u32 reg);
  void ewr(u32 reg, u32 val);

  int eeprom_read_16(u16 off);
  int eeprom_read(u16 *buf, int off, int count);

  void cleantx();
  void allocrx();

  void cleanrx();

  void reset();

protected:
  void handle_irq();

public:
  e1000(const struct e1000_model *model, struct pci_func *pcif);
  e1000(const e1000 &) = delete;
  e1000 &operator=(const e1000 &) = delete;

  static int attach(struct pci_func *pcif);

  bool valid() const
  {
    return valid_;
  }

  int transmit(void *buf, uint32_t len);
  void get_hwaddr(uint8_t *hwaddr);
};

struct eerd {
  u32 data_shift;
  u32 addr_shift;
  u32 done_bit;
  u32 start_bit;
};

static const struct eerd eerd_default = {
  data_shift: 16,
  addr_shift: 8,
  done_bit:   0x00000010,
  start_bit:  0x00000001,
};

// 82541xx, 82571xx, 82572xx, all PCI-E
static const struct eerd eerd_large = {
  data_shift: 16,
  addr_shift: 2,
  done_bit:   0x00000002,
  start_bit:  0x00000001
};

enum {
  MODEL_FLAG_DUAL_PORT = 1 << 0,
  MODEL_FLAG_PCIE = 1 << 1,
};

static struct e1000_model
{
  const char *name;
  int devid;
  const struct eerd *eerd;
  int flags;
} e1000_models[] = {
  {
    // QEMU's E1000 model
    "82540EM (desktop)", 0x100e,
    &eerd_default,
  }, {
    // josmp
    "82546GB (copper; dual port)", 0x1079,
    &eerd_default,
    MODEL_FLAG_DUAL_PORT,
  },
  // This is disabled on tom because it interferes with the IPMI
  // interface.  We don't need it because tom also has an E1000e
  // (below).
#ifndef HW_tom
  {
    // tom
    "82541PI (copper)", 0x1076,
    &eerd_large,
  },
#endif
  {
    // ud0
    "82573E (copper)", 0x108c,
    &eerd_large,
    MODEL_FLAG_PCIE,
  }, {
    // Also ud0
    "82573L", 0x100a,
    &eerd_large,
    MODEL_FLAG_PCIE,
  }, {
    // tom and ben
    "82572EI (copper)", 0x107d,
    &eerd_large,
    MODEL_FLAG_PCIE,
  },
};

u32
e1000::erd(u32 reg)
{
  paddr pa = membase_ + reg;
  volatile u32 *ptr = (u32*) p2v(pa);
  return *ptr;
}

void
e1000::ewr(u32 reg, u32 val)
{
  paddr pa = membase_ + reg;
  volatile u32 *ptr = (u32*) p2v(pa);
  *ptr = val;
}

int
e1000::eeprom_read_16(u16 off)
{
  const eerd* eerd = model_->eerd;
  u32 reg;
  int x;

  // [E1000 13.4.4] Ensure EEC control is released
  reg = erd(WMREG_EECD);
  reg &= ~(EECD_EE_REQ|EECD_EE_GNT);
  ewr(WMREG_EECD, reg);

  // [E1000 5.3.1] Software EEPROM access 
  ewr(WMREG_EERD, (off<<eerd->addr_shift) | eerd->start_bit);
  for (x = 0; x < 5; x++) {
    reg = erd(WMREG_EERD);
    if (reg & eerd->done_bit)
      return (reg&EERD_DATA_MASK) >> eerd->data_shift;
    microdelay(50000);
  }
  return -1;
}

int
e1000::eeprom_read(u16 *buf, int off, int count)
{
  for (int i = 0; i < count; i++) {
    int r = eeprom_read_16(off+i);
    if (r < 0) {
      cprintf("eeprom_read: cannot read\n");
      return -1;
    }
    buf[i] = r;
  }
  return 0;
}

int
e1000::transmit(void *buf, u32 len)
{
  struct wiseman_txdesc *desc;
  u32 tail;

  scoped_acquire l(&lk_);
  // WMREG_TDT should only equal WMREG_TDH when we have
  // nothing to transmit.  Therefore, we can accomodate
  // TX_RING_SIZE-1 buffers.
  if (txinuse_ == TX_RING_SIZE-1) {
    cprintf("TX ring overflow\n");
    return -1;
  }

  tail = erd(WMREG_TDT);
  desc = &txd_[tail];
  if (!(desc->wtx_fields.wtxu_status & WTX_ST_DD))
    panic("e1000tx");

  desc->wtx_addr = v2p(buf);
  desc->wtx_cmdlen = len | WTX_CMD_RS | WTX_CMD_EOP | WTX_CMD_IFCS;
  memset(&desc->wtx_fields, 0, sizeof(desc->wtx_fields));
  ewr(WMREG_TDT, (tail+1) % TX_RING_SIZE);
  txinuse_++;

  return 0;
}

void
e1000::cleantx()
{
  struct wiseman_txdesc *desc;
  void *va;

  scoped_acquire l(&lk_);
  while (txinuse_) {
    desc = &txd_[txclean_];
    if (!(desc->wtx_fields.wtxu_status & WTX_ST_DD))
      break;

    va = p2v(desc->wtx_addr);
    netfree(va);
    desc->wtx_fields.wtxu_status = WTX_ST_DD;

    txclean_ = (txclean_+1) % TX_RING_SIZE;
    desc = &txd_[txclean_];
    txinuse_--;
  }
}

void
e1000::allocrx()
{
  struct wiseman_rxdesc *desc;
  void *buf;
  u32 i;

  i = erd(WMREG_RDT);
  desc = &rxd_[i];
  if (desc->wrx_status & WRX_ST_DD)
    panic("allocrx");
  buf = netalloc();
  if (buf == nullptr)
    panic("Oops");
  desc->wrx_addr = v2p(buf);

  ewr(WMREG_RDT, (i+1) % RX_RING_SIZE);
}

void
e1000::cleanrx()
{
  struct wiseman_rxdesc *desc;
  void *va;
  u16 len;

  acquire(&lk_);
  desc = &rxd_[rxclean_];
  while (desc->wrx_status & WRX_ST_DD) {
    va = p2v(desc->wrx_addr);
    len = desc->wrx_len;

    desc->wrx_status = 0;
    allocrx();

    rxclean_ = (rxclean_+1) % RX_RING_SIZE;

    release(&lk_);
    netrx(va, len);
    acquire(&lk_);

    desc = &rxd_[rxclean_];
  }
  release(&lk_);
}

void
e1000::handle_irq()
{
  u32 icr = erd(WMREG_ICR);

  while (icr & (ICR_TXDW|ICR_RXO|ICR_RXT0)) {
    if (icr & ICR_TXDW)
      cleantx();
	
    if (icr & ICR_RXT0)
      cleanrx();

    if (icr & ICR_RXO)
      panic("ICR_RXO");

    icr = erd(WMREG_ICR);
  }
}

void
e1000::get_hwaddr(uint8_t *hwaddr)
{
  memmove(hwaddr, hwaddr_, sizeof(hwaddr_));
}

void
e1000::reset()
{
  u32 ctrl;
  paddr tpa;
  paddr rpa;

  ctrl = erd(WMREG_CTRL);  
  // [E1000 13.4.1] Assert PHY_RESET then delay as much as 10 msecs
  // before clearing PHY_RESET.
  ewr(WMREG_CTRL, ctrl|CTRL_PHY_RESET);
  microdelay(10000);
  ewr(WMREG_CTRL, ctrl);

  // [E1000 13.4.1] Delay 1 usec after reset
  ewr(WMREG_CTRL, ctrl|CTRL_RST);
  microdelay(1);

  // [E1000 13.4.41] Transmit Interrupt Delay Value of 1 usec.
  // A value of 0 is not allowed.  Enabled on a per-TX decriptor basis.
  ewr(WMREG_TIDV, 1);
  // [E1000 13.4.44] Delay TX interrupts a max of 1 usec.
  ewr(WMREG_TADV, 1);
  for (int i = 0; i < TX_RING_SIZE; i++)
    txd_[i].wtx_fields.wtxu_status = WTX_ST_DD;
  // [E1000 14.5] Transmit Initialization
  tpa = v2p(txd_);
  ewr(WMREG_TDBAH, tpa >> 32);
  ewr(WMREG_TDBAL, tpa & 0xffffffff);
  ewr(WMREG_TDLEN, sizeof(txd_));
  ewr(WMREG_TDH, 0);
  ewr(WMREG_TDT, 0);
  ewr(WMREG_TCTL, TCTL_EN|TCTL_PSP|TCTL_CT(0x10)|TCTL_COLD(0x40));
  ewr(WMREG_TIPG, TIPG_IPGT(10)|TIPG_IPGR1(8)|TIPG_IPGR2(6));

  for (int i = 0; i < RX_RING_SIZE>>1; i++) {
    void *buf = netalloc();
    rxd_[i].wrx_addr = v2p(buf);
  }
  rpa = v2p(rxd_);
  ewr(WMREG_RDBAH, rpa >> 32);
  ewr(WMREG_RDBAL, rpa & 0xffffffff);
  ewr(WMREG_RDLEN, sizeof(rxd_));
  ewr(WMREG_RDH, 0);
  ewr(WMREG_RDT, RX_RING_SIZE>>1);
  ewr(WMREG_RDTR, 0);
  ewr(WMREG_RADV, 0);
  ewr(WMREG_RCTL,
      RCTL_EN | RCTL_RDMTS_1_2 | RCTL_DPF | RCTL_BAM | RCTL_2k);

  // [E1000 13.4.20]
  ewr(WMREG_IMC, ~0);
  ewr(WMREG_IMS, ICR_TXDW | ICR_RXO | ICR_RXT0);
}

int
e1000::attach(struct pci_func *pcif)
{
  if (the_netdev)
    return 0;

  int devid = PCI_PRODUCT(pcif->dev_id);
  e1000_model *model = nullptr;
  for (auto &m : e1000_models) {
    if (devid == m.devid) {
      model = &m;
      break;
    }
  }
  if (!model)
    panic("e1000attach: unrecognized devid %x", devid);
  console.println("e1000: Found ", model->name);

  if (model->flags & MODEL_FLAG_PCIE) {
    console.println("e1000: PCI-E not implemented");
    return 0;
  }

  // On dual-ported devices, PCI functions 0 and 1 are ports 0 and 1.
  if ((model->flags & MODEL_FLAG_DUAL_PORT) && pcif->func != E1000_PORT)
    return 0;

  pci_func_enable(pcif);

  class e1000 *e1000 = new class e1000(model, pcif);
  if (!e1000->valid()) {
    delete e1000;
    return 0;
  }

  the_netdev = e1000;

  return 1;
}

e1000::e1000(const struct e1000_model *model, struct pci_func *pcif)
  : model_(model), membase_(pcif->reg_base[0]), iobase_(pcif->reg_base[2]),
    lk_("e1000", true), valid_(false)
{
  irq e1000irq = extpic->map_pci_irq(pcif);
  e1000irq.register_handler(this);
  e1000irq.enable();

  reset();

  // Get the MAC address
  int r = eeprom_read((u16*)hwaddr_, EEPROM_OFF_MACADDR, 3);
  if (r < 0)
    return;

  if ((model_->flags & MODEL_FLAG_DUAL_PORT) && E1000_PORT)
    // [E1000 12.3.1] The EEPROM is shared, so we read port 0's MAC
    // address.  Port 1's is the same with the LSB inverted.
    hwaddr_[5] ^= 1;

  if (VERBOSE)
      cprintf("%x:%x:%x:%x:%x:%x\n",
              hwaddr_[0], hwaddr_[1], hwaddr_[2],
              hwaddr_[3], hwaddr_[4], hwaddr_[5]);

  u32 ralow = ((u32) hwaddr_[0]) | ((u32) hwaddr_[1] << 8) |
      ((u32) hwaddr_[2] << 16) | ((u32) hwaddr_[3] << 24);
  u32 rahigh = ((u32) hwaddr_[4] | ((u32) hwaddr_[5] << 8));
  
  ewr(WMREG_RAL_LO(WMREG_CORDOVA_RAL_BASE, 0), ralow);
  erd(WMREG_STATUS);
  ewr(WMREG_RAL_HI(WMREG_CORDOVA_RAL_BASE, 0), rahigh|RAL_AV);
  erd(WMREG_STATUS);

  for (int i = 0; i < WMREG_MTA; i+=4)
    ewr(WMREG_CORDOVA_MTA+i, 0);

  valid_ = true;
}

void
inite1000(void)
{
  for (auto &m : e1000_models)
    pci_register_driver(0x8086, m.devid, e1000::attach);
}
