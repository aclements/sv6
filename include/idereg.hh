#pragma once

// Device register bits
#define IDE_DEV_LBA     0x40

// Device control register (write to control block register) bits
#define IDE_CTL_LBA48   0x80
#define IDE_CTL_SRST    0x04
#define IDE_CTL_NIEN    0x02

// Error register bits
#define IDE_ERR_BBK     0x80    // bad block
#define IDE_ERR_CRC     0x80    // CRC error (UDMA)
#define IDE_ERR_UNC     0x40    // uncorrectable data
#define IDE_ERR_MC      0x20    // media changed
#define IDE_ERR_IDNF    0x10    // ID not found
#define IDE_ERR_MCR     0x08    // media change requested
#define IDE_ERR_ABRT    0x04    // aborted command
#define IDE_ERR_TK0NF   0x02    // track 0 not found
#define IDE_ERR_AMNF    0x01    // address mark not found

// Status bits
#define IDE_STAT_BSY    0x80
#define IDE_STAT_DRDY   0x40
#define IDE_STAT_DF     0x20
#define IDE_STAT_DRQ    0x08
#define IDE_STAT_ERR    0x01

// Command register values
#define IDE_CMD_READ            0x20
#define IDE_CMD_READ_DMA        0xc8
#define IDE_CMD_READ_DMA_EXT    0x25
#define IDE_CMD_WRITE           0x30
#define IDE_CMD_WRITE_DMA       0xca
#define IDE_CMD_WRITE_DMA_EXT   0x35
#define IDE_CMD_FLUSH_CACHE     0xe7
#define IDE_CMD_IDENTIFY        0xec
#define IDE_CMD_SETFEATURES     0xef

// Feature bits
#define IDE_FEATURE_WCACHE_ENA  0x02
#define IDE_FEATURE_XFER_MODE   0x03
#define IDE_FEATURE_RLA_DIS     0x55
#define IDE_FEATURE_WCACHE_DIS  0x82
#define IDE_FEATURE_RLA_ENA     0xAA

// Identify device structure
struct identify_device {
  u16 pad0[10];           // Words 0-9
  char serial[20];        // Words 10-19
  u16 pad1[3];            // Words 20-22
  char firmware[8];       // Words 23-26
  char model[40];         // Words 27-46
  u16 pad2[13];           // Words 47-59
  u32 lba_sectors;        // Words 60-61, assuming little-endian
  u16 pad3[24];           // Words 62-85
  u16 features86;         // Word 86
  u16 features87;         // Word 87
  u16 udma_mode;          // Word 88
  u16 pad4[4];            // Words 89-92
  u16 hwreset;            // Word 93
  u16 pad5[6];            // Words 94-99
  u64 lba48_sectors;      // Words 100-104, assuming little-endian
};

#define IDE_FEATURE86_LBA48     (1 << 10)
#define IDE_HWRESET_CBLID       0x2000

