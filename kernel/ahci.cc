#include <kern/disk.h>
#include <dev/ahci.h>
#include <dev/pcireg.h>
#include <dev/ahcireg.h>
#include <dev/idereg.h>
#include <dev/satareg.h>
#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/timer.h>
#include <kern/intr.h>
#include <inc/error.h>

enum { fis_debug = 0 };

struct ahci_port_page {
    volatile struct ahci_recv_fis rfis;		/* 256-byte alignment */
    uint8_t pad[0x300];

    volatile struct ahci_cmd_header cmdh;	/* 1024-byte alignment */
    struct ahci_cmd_header cmdh_unused[31];

    volatile struct ahci_cmd_table cmdt;	/* 128-byte alignment */

    struct disk dk;
    disk_callback cb;
    void *cbarg;
};

struct ahci_hba {
    uint32_t irq;
    uint32_t membase;
    volatile struct ahci_reg *r;
    struct ahci_port_page *port[32];
    struct interrupt_handler ih;
};

/*
 * Helper functions
 */

static uint32_t
ahci_fill_prd(struct ahci_hba *a, uint32_t port,
	      struct kiovec *iov_buf, uint32_t iov_cnt)
{
    uint32_t nbytes = 0;

    struct ahci_cmd_table *cmd = (void *) &a->port[port]->cmdt;
    assert(iov_cnt < sizeof(cmd->prdt) / sizeof(cmd->prdt[0]));

    for (uint32_t slot = 0; slot < iov_cnt; slot++) {
	cmd->prdt[slot].dba = kva2pa(iov_buf[slot].iov_base);
	cmd->prdt[slot].dbc = iov_buf[slot].iov_len - 1;
	nbytes += iov_buf[slot].iov_len;
    }

    a->port[port]->cmdh.prdtl = iov_cnt;
    return nbytes;
}

static void
ahci_fis_debug(struct sata_fis_reg *r)
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

static void
ahci_fill_fis(struct ahci_hba *a, uint32_t port, void *fis, uint32_t fislen)
{
    memcpy((void *) &a->port[port]->cmdt.cfis[0], fis, fislen);
    a->port[port]->cmdh.flags = fislen / sizeof(uint32_t);
    if (fis_debug)
        ahci_fis_debug((struct sata_fis_reg *) fis);
}

static void
ahci_port_debug(struct ahci_hba *a, uint32_t port)
{
    cprintf("AHCI port %d dump:\n", port);
    cprintf("PxCMD    = 0x%x\n", a->r->port[port].cmd);
    cprintf("PxTFD    = 0x%x\n", a->r->port[port].tfd);
    cprintf("PxSIG    = 0x%x\n", a->r->port[port].sig);
    cprintf("PxCI     = 0x%x\n", a->r->port[port].ci);
    cprintf("SStatus  = 0x%x\n", a->r->port[port].ssts);
    cprintf("SControl = 0x%x\n", a->r->port[port].sctl);
    cprintf("SError   = 0x%x\n", a->r->port[port].serr);
    cprintf("GHC      = 0x%x\n", a->r->ghc);
}

static int
ahci_port_wait(struct ahci_hba *a, uint32_t port)
{
    uint64_t ts_start = karch_get_tsc();
    for (;;) {
	uint32_t tfd = a->r->port[port].tfd;
	uint8_t stat = AHCI_PORT_TFD_STAT(tfd);
	if (!(stat & IDE_STAT_BSY) && !(a->r->port[port].ci & 1))
	    return 0;

	uint64_t ts_diff = karch_get_tsc() - ts_start;
	if (ts_diff > 1000 * 1000 * 1000) {
	    cprintf("ahci_port_wait: stuck for %"PRIu64" cycles\n", ts_diff);
	    ahci_port_debug(a, port);
	    return -1;
	}
    }
}

/*
 * Driver hooks.
 */

static void
ahci_complete(struct ahci_hba *a, uint32_t port)
{
    assert(a->port[port]->cb && !(a->r->port[port].ci & 1));
    uint32_t tfd = a->r->port[port].tfd;

    disk_io_status stat = disk_io_success;
    if (AHCI_PORT_TFD_STAT(tfd) & (IDE_STAT_ERR | IDE_STAT_DF)) {
	cprintf("ahci_complete: status %02x, err %02x\n",
		AHCI_PORT_TFD_STAT(tfd), AHCI_PORT_TFD_ERR(tfd));
	stat = disk_io_failure;
    }

    disk_callback cb = a->port[port]->cb;
    a->port[port]->cb = 0;
    cb(stat, a->port[port]->cbarg);
}

static void
ahci_intr(void *arg)
{
    struct ahci_hba *a = arg;
    a->r->is = ~0;

    for (uint32_t i = 0; i < 32; i++) {
	a->r->port[i].is = ~0;
	if (a->port[i] && a->port[i]->cb && !(a->r->port[i].ci & 1))
	    ahci_complete(a, i);
    }
}

static void
ahci_poll(struct disk *dk)
{
    ahci_intr(dk->dk_arg);
}

static int
ahci_issue(struct disk *dk, disk_op op, struct kiovec *iov_buf, int iov_cnt,
	   uint64_t offset, disk_callback cb, void *cbarg)
{
    assert(!(offset % 512));
    uint64_t sector_off = offset / 512;

    struct ahci_hba *a = dk->dk_arg;
    uint32_t port = dk->dk_id;
    if (a->port[port]->cb)
	return -E_BUSY;

    struct sata_fis_reg fis;
    memset(&fis, 0, sizeof(fis));
    fis.type = SATA_FIS_TYPE_REG_H2D;
    fis.cflag = SATA_FIS_REG_CFLAG;
    fis.command = SAFE_EQUAL(op, op_read)  ? IDE_CMD_READ_DMA_EXT :
		  SAFE_EQUAL(op, op_write) ? IDE_CMD_WRITE_DMA_EXT :
		  SAFE_EQUAL(op, op_flush) ? IDE_CMD_FLUSH_CACHE : 0;

    if (!SAFE_EQUAL(op, op_flush)) {
	uint32_t len = ahci_fill_prd(a, port, iov_buf, iov_cnt);
	assert(!(len % 512));
	assert(len <= DISK_REQMAX);

	fis.dev_head = IDE_DEV_LBA;
	fis.control = IDE_CTL_LBA48;

	fis.sector_count = len / 512;
	fis.lba_0 = (sector_off >>  0) & 0xff;
	fis.lba_1 = (sector_off >>  8) & 0xff;
	fis.lba_2 = (sector_off >> 16) & 0xff;
	fis.lba_3 = (sector_off >> 24) & 0xff;
	fis.lba_4 = (sector_off >> 32) & 0xff;
	fis.lba_5 = (sector_off >> 40) & 0xff;
	ahci_fill_fis(a, port, &fis, sizeof(fis));

	if (SAFE_EQUAL(op, op_read)) {
	    a->port[port]->cmdh.prdbc = 0;
	} else {
	    a->port[port]->cmdh.flags |= AHCI_CMD_FLAGS_WRITE;
	    a->port[port]->cmdh.prdbc = len;
	}
    }

    a->r->port[port].ci |= 1;
    a->port[port]->cb = cb;
    a->port[port]->cbarg = cbarg;
    return 0;
}

/*
 * Initialization.
 */

static void
ahci_reset_port(struct ahci_hba *a, uint32_t port)
{
    /* Wait for port to quiesce */
    if (a->r->port[port].cmd & (AHCI_PORT_CMD_ST | AHCI_PORT_CMD_CR |
				AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_FR)) {
	cprintf("AHCI: port %d active, clearing..\n", port);
	a->r->port[port].cmd &= ~(AHCI_PORT_CMD_ST | AHCI_PORT_CMD_FRE);
	timer_delay(500 * 1000 * 1000);

	if (a->r->port[port].cmd & (AHCI_PORT_CMD_CR | AHCI_PORT_CMD_FR)) {
	    cprintf("AHCI: port %d still active, giving up\n", port);
	    return;
	}
    }

    /* Initialize memory buffers */
    a->port[port]->cmdh.ctba = kva2pa((void *) &a->port[port]->cmdt);
    a->r->port[port].clb = kva2pa((void *) &a->port[port]->cmdh);
    a->r->port[port].fb = kva2pa((void *) &a->port[port]->rfis);
    a->r->port[port].ci = 0;

    /* Clear any errors first, otherwise the chip wedges */
    a->r->port[port].serr = ~0;
    a->r->port[port].serr = 0;

    /* Enable receiving frames */
    a->r->port[port].cmd |= AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_ST |
			    AHCI_PORT_CMD_SUD | AHCI_PORT_CMD_POD |
			    AHCI_PORT_CMD_ACTIVE;

    /* Check if there's anything there */
    uint32_t phystat = a->r->port[port].ssts;
    if (!phystat) {
	cprintf("AHCI: port %d: not connected\n", port);
	return;
    }

    /* Try to send an IDENTIFY */
    static union {
	struct identify_device id;
	char buf[512];
    } id_buf;

    struct kiovec id_iov =
	{ .iov_base = &id_buf.buf[0], .iov_len = sizeof(id_buf) };

    struct sata_fis_reg fis;
    memset(&fis, 0, sizeof(fis));
    fis.type = SATA_FIS_TYPE_REG_H2D;
    fis.cflag = SATA_FIS_REG_CFLAG;
    fis.command = IDE_CMD_IDENTIFY;
    fis.sector_count = 1;

    ahci_fill_prd(a, port, &id_iov, 1);
    ahci_fill_fis(a, port, &fis, sizeof(fis));
    a->r->port[port].ci |= 1;

    int r = ahci_port_wait(a, port);
    if (r < 0) {
	cprintf("AHCI: port %d: cannot identify\n", port);
	return;
    }

    /* Fill in the disk object */
    struct disk *dk = &a->port[port]->dk;
    dk->dk_arg = a;
    dk->dk_id = port;
    dk->dk_issue = &ahci_issue;
    dk->dk_poll = &ahci_poll;

    if (!(id_buf.id.features86 & IDE_FEATURE86_LBA48)) {
	cprintf("AHCI: disk too small, driver requires LBA48\n");
	return;
    }

    uint64_t sectors = id_buf.id.lba48_sectors;
    dk->dk_bytes = sectors * 512;
    memcpy(&dk->dk_model[0], id_buf.id.model, sizeof(id_buf.id.model));
    memcpy(&dk->dk_serial[0], id_buf.id.serial, sizeof(id_buf.id.serial));
    memcpy(&dk->dk_firmware[0], id_buf.id.firmware, sizeof(id_buf.id.firmware));
    static_assert(sizeof(dk->dk_model) >= sizeof(id_buf.id.model));
    static_assert(sizeof(dk->dk_serial) >= sizeof(id_buf.id.serial));
    static_assert(sizeof(dk->dk_firmware) >= sizeof(id_buf.id.firmware));
    sprintf(&dk->dk_busloc[0], "ahci.%d", port);

    /* Enable write-caching, read look-ahead */
    memset(&fis, 0, sizeof(fis));
    ahci_fill_prd(a, port, 0, 0);

    fis.type = SATA_FIS_TYPE_REG_H2D;
    fis.cflag = SATA_FIS_REG_CFLAG;
    fis.command = IDE_CMD_SETFEATURES;

    fis.features = IDE_FEATURE_WCACHE_ENA;
    ahci_fill_fis(a, port, &fis, sizeof(fis));
    a->r->port[port].ci |= 1;
    r = ahci_port_wait(a, port);
    if (r < 0) {
	cprintf("AHCI: port %d: cannot enable write caching\n", port);
	return;
    }

    fis.features = IDE_FEATURE_RLA_ENA;
    ahci_fill_fis(a, port, &fis, sizeof(fis));
    a->r->port[port].ci |= 1;
    r = ahci_port_wait(a, port);
    if (r < 0) {
	cprintf("AHCI: port %d: cannot enable read lookahead\n", port);
	return;
    }

    /* Enable interrupts and done */
    a->r->port[port].ie = AHCI_PORT_INTR_DHRE;
    disk_register(dk);
}

static void
ahci_reset(struct ahci_hba *a)
{
    a->r->ghc |= AHCI_GHC_AE | AHCI_GHC_IE;

    for (uint32_t i = 0; i < 32; i++)
	if (a->r->pi & (1 << i))
	    ahci_reset_port(a, i);
}

int
ahci_init(struct pci_func *f)
{
    if (PCI_INTERFACE(f->dev_class) != 0x01) {
	cprintf("ahci_init: not an AHCI controller\n");
	return 0;
    }

    struct ahci_hba *a;
    int r = page_alloc((void **) &a);
    if (r < 0)
	return r;

    static_assert(sizeof(*a) <= PGSIZE);
    memset(a, 0, PGSIZE);

    for (int i = 0; i < 32; i++) {
	static_assert(sizeof(a->port[i]) <= PGSIZE);
	r = page_alloc((void **) &a->port[i]);
	if (r < 0)
	    return r;
	memset((void *) a->port[i], 0, PGSIZE);
    }

    pci_func_enable(f);
    a->irq = f->irq_line;
    a->membase = f->reg_base[5];
    a->r = pa2kva(a->membase);

    cprintf("AHCI: base 0x%x, irq %d, v 0x%x\n",
	    a->membase, a->irq, a->r->vs);
    ahci_reset(a);

    a->ih.ih_func = &ahci_intr;
    a->ih.ih_arg = a;
    irq_register(a->irq, &a->ih);

    return 1;
}
