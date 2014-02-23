#pragma once

/*
 * Originally from HiStar kern/dev/ahcireg.h
 */

#include "satareg.hh"

struct ahci_reg_global {
  u32 cap;		/* host capabilities */
  u32 ghc;		/* global host control */
  u32 is;		/* interrupt status */
  u32 pi;		/* ports implemented */
  u32 vs;		/* version */
  u32 ccc_ctl;		/* command completion coalescing control */
  u32 ccc_ports;	/* command completion coalescing ports */
  u32 em_loc;		/* enclosure management location */
  u32 em_ctl;		/* enclosure management control */
  u32 cap2;		/* extended host capabilities */
  u32 bohc;		/* BIOS/OS handoff control and status */
};

#define AHCI_GHC_AE		(1 << 31)
#define AHCI_GHC_IE		(1 << 1)
#define AHCI_GHC_HR		(1 << 0)

struct ahci_reg_port {
  u64 clb;		/* command list base address */
  u64 fb;		/* FIS base address */
  u32 is;		/* interrupt status */
  u32 ie;		/* interrupt enable */
  u32 cmd;		/* command and status */
  u32 reserved;
  u32 tfd;		/* task file data */
  u32 sig;		/* signature */
  u32 ssts;		/* sata phy status: SStatus */
  u32 sctl;		/* sata phy control: SControl */
  u32 serr;		/* sata phy error: SError */
  u32 sact;		/* sata phy active: SActive */
  u32 ci;		/* command issue */
  u32 sntf;		/* sata phy notification: SNotify */
  u32 fbs;		/* FIS-based switching control */
};

#define AHCI_PORT_CMD_ST	(1 << 0)	/* start */
#define AHCI_PORT_CMD_SUD	(1 << 1)	/* spin-up device */
#define AHCI_PORT_CMD_POD	(1 << 2)	/* power on device */
#define AHCI_PORT_CMD_FRE	(1 << 4)	/* FIS receive enable */
#define AHCI_PORT_CMD_FR	(1 << 14)	/* FIS receive running */
#define AHCI_PORT_CMD_CR	(1 << 15)	/* command list running */
#define AHCI_PORT_CMD_ACTIVE	(1 << 28)	/* ICC active */
#define AHCI_PORT_TFD_ERR(tfd)	(((tfd) >> 8) & 0xff)
#define AHCI_PORT_TFD_STAT(tfd)	(((tfd) >> 0) & 0xff)
#define AHCI_PORT_SCTL_RESET	0x01
#define AHCI_PORT_INTR_DHRE	(1 << 0)

struct ahci_reg {
  union {
    struct ahci_reg_global g;
    char __pad[0x100];
  };

  struct {
    union {
      struct ahci_reg_port p;
      char __pad[0x80];
    };
  } port[32];
};

struct ahci_recv_fis {
  u8 dsfis[0x20];	/* DMA setup FIS */
  u8 psfis[0x20];	/* PIO setup FIS */
  sata_fis_reg reg;	/* D2H register FIS */
  u8 __pad[0x4];
  u8 sdbfis[0x8];	/* set device bits FIS */
  u8 ufis[0x40];	/* unknown FIS */
  u8 reserved[0x60];
};

struct ahci_cmd_header {
  u16 flags;
  u16 prdtl;
  u32 prdbc;
  u64 ctba;
  u64 reserved0;
  u64 reserved1;
};

#define AHCI_CMD_FLAGS_WRITE	(1 << 6)
#define AHCI_CMD_FLAGS_CFL_MASK	0x1f		/* command FIS len, in DWs */

struct ahci_prd {
  u64 dba;
  u32 reserved;
  u32 dbc;		/* one less than #bytes */
};

struct ahci_cmd_table {
  u8 cfis[0x40];		/* command FIS */
  u8 acmd[0x10];		/* ATAPI command */
  u8 reserved[0x30];
  ahci_prd prdt[DISK_REQMAX / PGSIZE + 1];
};
