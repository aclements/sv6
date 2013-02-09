// Intel 8250 serial port (UART).
// http://en.wikibooks.org/wiki/Serial_Programming/8250_UART_Programming

#include "types.h"
#include "kernel.hh"
#include "amd64.h"
#include "traps.h"
#include "apic.hh"
#include "irq.hh"

#define COM2    0x2f8
#define COM1    0x3f8

#define COM_IN_RECEIVE             0      // DLAB = 0, in
#define COM_OUT_TRANSMIT           0      // DLAB = 0, out
#define COM_INT_EN                 1      // DLAB = 0
# define COM_INT_RECEIVE            0x01
# define COM_INT_TRANSMIT           0x02
# define COM_INT_LINE               0x04
# define COM_INT_MODEM              0x08
#define COM_DIVISOR_LSB            0      // DLAB = 1
#define COM_DIVISOR_MSB            1      // DLAB = 1
#define COM_IN_IIR                 2
# define COM_IN_IIR_NOT_PENDING     0x01
# define COM_IN_IIR_ID_MASK         0x0E
#define COM_OUT_FIFO_CTL           2
# define COM_OUT_FIFO_ENABLE        0x01
# define COM_OUT_FIFO_RECV_RESET    0x02
# define COM_OUT_FIFO_XMIT_RESET    0x04
# define COM_OUT_FIFO_DMA           0x08
# define COM_OUT_FIFO_TRIGGER_MASK  0xC0
# define COM_OUT_FIFO_TRIGGER_1     (0<<6)
# define COM_OUT_FIFO_TRIGGER_4     (1<<6)
# define COM_OUT_FIFO_TRIGGER_8     (2<<6)
# define COM_OUT_FIFO_TRIGGER_14    (3<<6)
#define COM_LINE_CTL               3
# define COM_LINE_LEN_MASK          0x03
# define COM_LINE_LEN_5             0
# define COM_LINE_LEN_6             1
# define COM_LINE_LEN_7             2
# define COM_LINE_LEN_8             3
# define COM_LINE_STOP_BITS         0x04
# define COM_LINE_PARITY            0x08
# define COM_LINE_EVEN_PARITY       0x10
# define COM_LINE_STICK_PARITY      0x20
# define COM_LINE_BREAK_CTL         0x40
# define COM_LINE_DLAB              0x80
#define COM_MODEM_CTL              4
#define COM_LINE_STATUS            5
# define COM_LINE_DATA_READY        0x01
# define COM_LINE_OVERRUN_ERR       0x02
# define COM_LINE_PARITY_ERR        0x04
# define COM_LINE_FRAMING_ERR       0x08
# define COM_LINE_BREAK             0x10
# define COM_LINE_XMIT_HOLDING      0x20
# define COM_LINE_XMIT_EMPTY        0x40
# define COM_LINE_FIFO_ERR          0x80
#define COM_MODEM_STATUS           6
#define COM_SCRATCH                7

static int com;
static int irq_com;
static int uart;    // is there a uart?

void
uartputc(char c)
{
  int i;

  if (!uart)
    return;
  for (i = 0; i < 128 && !(inb(com+COM_LINE_STATUS) & COM_LINE_XMIT_HOLDING);
       i++)
    microdelay(10);
#ifdef UART_SEND_DELAY_USEC
  microdelay(UART_SEND_DELAY_USEC);
#endif
  outb(com+COM_OUT_TRANSMIT, c);
#ifdef UART_SEND_DELAY_USEC
  microdelay(UART_SEND_DELAY_USEC);
#endif
}

static int
uartgetc(void)
{
  if (!uart)
    return -1;
  if (!(inb(com+COM_LINE_STATUS) & COM_LINE_DATA_READY))
    return -1;
  return inb(com+COM_IN_RECEIVE);
}

void
uartintr(void)
{
  consoleintr(uartgetc);
}

void
inituart(void)
{
  static struct {
    int com;
    int irq;
  } conf[] = {
    // Try COM2 (aka ttyS1) first, because it usually does SOL for IPMI.
    { COM2, IRQ_COM2 },
    // Still try COM1 (aka ttyS0), because it is what QEMU emulates.
    { COM1, IRQ_COM1 },
  };

  int i;
#if defined(UART_BAUD)
  int baud = UART_BAUD;
#else
  int baud = 19200;
#endif
  for (i = 0; i < 2; i++) {
    com = conf[i].com;
    irq_com = conf[i].irq;

    // Turn off the FIFO
    outb(com+COM_OUT_FIFO_CTL, 0);
    // 19200 baud
    outb(com+COM_LINE_CTL, COM_LINE_DLAB);    // Unlock divisor
    outb(com+COM_DIVISOR_LSB, 115200/baud);
    outb(com+COM_DIVISOR_MSB, 0);
    // 8 bits, one stop bit, no parity
    outb(com+COM_LINE_CTL, COM_LINE_LEN_8); // Lock divisor, 8 data bits.
    outb(com+COM_INT_EN, COM_INT_RECEIVE); // Enable receive interrupts.
    // Data terminal ready
    outb(com+COM_MODEM_CTL, 0x0);
    
    // If status is 0xFF, no serial port.
    if(inb(com+COM_LINE_STATUS) != 0xFF)
      break;
  }
  if (i == 2)
    return;

  uart = 1;

  // Clean up the serial console (beginning of line, erase down)
  for (const char *p="\r\x1b[J"; *p; p++)
    uartputc(*p);

  // Announce that we're here.
  for (const char *p=DEBUG?"xv6 DEBUG\n":"xv6\n"; *p; p++)
    uartputc(*p);
}

void
inituartcons(void)
{
  // Acknowledge pre-existing interrupt conditions;
  // enable interrupts.
  extpic->map_isa_irq(irq_com).enable();
  inb(com+COM_IN_IIR);
  inb(com+COM_IN_RECEIVE);
}
