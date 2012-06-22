// See MultiProcessor Specification Version 1.[14]

struct mp {             // floating pointer [MP 4.1]
  u8 signature[4];           // "_MP_"
  u32 physaddr;              // phys addr of MP config table
  u8 length;                 // 1
  u8 specrev;                // [14]
  u8 checksum;               // all bytes must add up to 0
  u8 type;                   // MP system config type
  u8 imcrp;
  u8 reserved[3];
} __attribute__((__packed__));

struct mpconf {         // configuration table header [MP 4.2]
  char signature[4];         // "PCMP"
  u16 length;                // total table length
  u8 version;                // [14]
  u8 checksum;               // all bytes must add up to 0
  char oemid[8];             // OEM ID string
  char product[12];          // product id
  u32 oemtable;              // OEM table pointer
  u16 oemlength;             // OEM table length
  u16 entry;                 // entry count
  u32 lapicaddr;             // address of local APIC
  u16 xlength;               // extended table length
  u8 xchecksum;              // extended table checksum
  u8 reserved;
  u8 entries[0];             // table entries
} __attribute__((__packed__));

struct mpproc {         // processor table entry [MP 4.3.1]
  u8 type;                   // entry type (0)
  u8 apicid;                 // local APIC id
  u8 version;                // local APIC version
  u8 flags;                  // CPU flags
    #define MPPROC_EN 0x01        // Enabled
    #define MPPROC_BP 0x02        // This proc is the bootstrap processor.
  u8 signature[4];           // CPU signature
  u32 feature;               // feature flags from CPUID instruction
  u8 reserved[8];
} __attribute__((__packed__));

struct mpbus {          // bus entry [MP 4.3.2]
  u8 type;                   // entry type (1)
  u8 busid;                  // bus id
  char name[6];              // bus type string
} __attribute__((__packed__));

struct mpioapic {       // I/O APIC table entry [MP 4.3.3]
  u8 type;                   // entry type (2)
  u8 apicid;                 // I/O APIC id
  u8 version;                // I/O APIC version
  u8 flags;                  // I/O APIC flags
    #define MPIOAPIC_EN 0x1       // Enable
  u32 addr;                  // I/O APIC address
} __attribute__((__packed__));

struct mpint {          // I/O or local interrupt assignment entry [MP 4.3.4]
  u8 type;                   // entry type (3=I/O, 4=Local)
  u8 inttype;                // interrupt type
                             // (0=INT, 1=NMI, 2=SMI, 3=ExtINT)
  u16 flag;                  // bits: 0-1: PO; 2-3: EL
  u8 busid;                  // source bus id
  u8 busirq;                 // source bus irq
  u8 apicno;                 // destination (L)APIC id
  u8 pinno;                  // destination (L)APIC (L)INTIN#
} __attribute__((__packed__));

// Table entry types
#define MPPROC    0x00  // One per processor
#define MPBUS     0x01  // One per bus
#define MPIOAPIC  0x02  // One per I/O APIC
#define MPIOINTR  0x03  // One per bus interrupt source
#define MPLINTR   0x04  // One per system interrupt source

