#pragma once

/*
 * Originally from HiStar kern/dev/satareg.h
 */

struct sata_fis_reg {
  u8 type;
  u8 cflag;
  union {
    u8 command;   /* H2D */
    u8 status;    /* D2H */
  };
  union {
    u8 features;  /* H2D */
    u8 error;     /* D2H */
  };

  u8 lba_0;
  u8 lba_1;
  u8 lba_2;
  u8 dev_head;

  u8 lba_3;
  u8 lba_4;
  u8 lba_5;
  u8 features_ex;

  u8 sector_count;
  u8 sector_count_ex;
  u8 __pad1;
  u8 control;

  u8 __pad2[4];
};

#define SATA_FIS_REG_CFLAG	(1 << 7)	/* issuing new command */

struct sata_fis_devbits {
  u8 type;
  u8 intr;
  u8 status;
  u8 error;
  u8 __pad[4];
};

#define SATA_FIS_TYPE_REG_H2D	0x27
#define SATA_FIS_TYPE_REG_D2H	0x34
#define SATA_FIS_TYPE_DEVBITS	0xA1	/* always D2H */
