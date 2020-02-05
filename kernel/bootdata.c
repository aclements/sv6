#include "types.h"
#include "mmu.h"
#include "kbd.h"

/*
 * Data structures that use C99 designated initializers,
 * which aren't avialable in C++11.
 */

struct segdesc  __attribute__((aligned(16))) bootgdt[NSEGS] = {
  // null
  [0]=SEGDESC(0, 0, 0),
  // 32-bit kernel code
  [1]=SEGDESC(0, 0xfffff, SEG_R|SEG_CODE|SEG_S|SEG_DPL(0)|SEG_P|SEG_D|SEG_G),
  // 64-bit kernel code
  [2]=SEGDESC(0, 0, SEG_R|SEG_CODE|SEG_S|SEG_DPL(0)|SEG_P|SEG_L|SEG_G),
  // kernel data
  [3]=SEGDESC(0, 0xfffff, SEG_W|SEG_S|SEG_DPL(0)|SEG_P|SEG_D|SEG_G),
  // The order of the user data and user code segments is
  // important for syscall instructions.  See initseg.
  // 64-bit user data
  [6]=SEGDESC(0, 0xfffff, SEG_W|SEG_S|SEG_DPL(3)|SEG_P|SEG_D|SEG_G),
  // 64-bit user code
  [7]=SEGDESC(0, 0, SEG_R|SEG_CODE|SEG_S|SEG_DPL(3)|SEG_P|SEG_L|SEG_G),
};

u8 kbd_shift_code[256] =
{
  [0x1D] = CTL,
  [0x2A] = SHIFT,
  [0x36] = SHIFT,
  [0x38] = ALT,
  [0x9D] = CTL,
  [0xB8] = ALT
};

u8 kbd_toggle_code[256] =
{
  [0x3A] = CAPSLOCK,
  [0x45] = NUMLOCK,
  [0x46] = SCROLLLOCK
};

u8 kbd_normal_map[256] =
{
  '\0', 0x1B, '1',  '2',  '3',  '4',  '5',  '6',  // 0x00
  '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
  'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  // 0x10
  'o',  'p',  '[',  ']',  '\n', '\0', 'a',  's',
  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  // 0x20
  '\'', '`',  '\0', '\\', 'z',  'x',  'c',  'v',
  'b',  'n',  'm',  ',',  '.',  '/',  '\0', '*',  // 0x30
  '\0', ' ',  '\0', '\0', '\0', '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '7',  // 0x40
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  '\0', '\0', '\0', '\0', // 0x50
  [0x9C] = '\n',      // KP_Enter
  [0xB5] = '/',       // KP_Div
  [0xC8] = KEY_UP,    [0xD0] = KEY_DN,
  [0xC9] = KEY_PGUP,  [0xD1] = KEY_PGDN,
  [0xCB] = KEY_LF,    [0xCD] = KEY_RT,
  [0x97] = KEY_HOME,  [0xCF] = KEY_END,
  [0xD2] = KEY_INS,   [0xD3] = KEY_DEL
};

u8 kbd_shift_map[256] =
{
  '\0', 033,  '!',  '@',  '#',  '$',  '%',  '^',  // 0x00
  '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
  'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  // 0x10
  'O',  'P',  '{',  '}',  '\n', '\0', 'A',  'S',
  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  // 0x20
  '"',  '~',  '\0', '|',  'Z',  'X',  'C',  'V',
  'B',  'N',  'M',  '<',  '>',  '?',  '\0', '*',  // 0x30
  '\0', ' ',  '\0', '\0', '\0', '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '7',  // 0x40
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  '\0', '\0', '\0', '\0', // 0x50
  [0x9C] = '\n',      // KP_Enter
  [0xB5] = '/',       // KP_Div
  [0xC8] = KEY_UP,    [0xD0] = KEY_DN,
  [0xC9] = KEY_PGUP,  [0xD1] = KEY_PGDN,
  [0xCB] = KEY_LF,    [0xCD] = KEY_RT,
  [0x97] = KEY_HOME,  [0xCF] = KEY_END,
  [0xD2] = KEY_INS,   [0xD3] = KEY_DEL
};

// C('A') == Control-A
#define C(x) (x - '@')

u8 kbd_ctl_map[256] =
{
  '\0',    '\0',    '\0',    '\0',    '\0',    '\0',    '\0',    '\0',
  '\0',    '\0',    '\0',    '\0',    '\0',    '\0',    '\0',    '\0',
  C('Q'),  C('W'),  C('E'),  C('R'),  C('T'),  C('Y'),  C('U'),  C('I'),
  C('O'),  C('P'),  '\0',    '\0',    '\r',    '\0',    C('A'),  C('S'),
  C('D'),  C('F'),  C('G'),  C('H'),  C('J'),  C('K'),  C('L'),  '\0',
  '\0',    '\0',    '\0',    C('\\'), C('Z'),  C('X'),  C('C'),  C('V'),
  C('B'),  C('N'),  C('M'),  '\0',    '\0',    C('/'),  '\0',    '\0',
  [0x9C] = '\r',      // KP_Enter
  [0xB5] = C('/'),    // KP_Div
  [0xC8] = KEY_UP,    [0xD0] = KEY_DN,
  [0xC9] = KEY_PGUP,  [0xD1] = KEY_PGDN,
  [0xCB] = KEY_LF,    [0xCD] = KEY_RT,
  [0x97] = KEY_HOME,  [0xCF] = KEY_END,
  [0xD2] = KEY_INS,   [0xD3] = KEY_DEL
};
