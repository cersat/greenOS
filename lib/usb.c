#include "usb.h"
#include "video.h"

typedef struct {
  u32 next;
  u32 alt_next;
  u32 token;
  u32 buf[5];
} __attribute__((packed)) qtd_t;

typedef struct {
  u32 hlp;
  u32 epchar;
  u32 epcap;
  u32 cur_qtd;
  u32 next_qtd;
  u32 alt_qtd;
  u32 token;
  u32 buf[5];
  u32 pad[4];
} __attribute__((packed)) qh_t;

static qh_t  ehci_qh       __attribute__((aligned(32)));
static u8    ehci_pad[32]  __attribute__((used));
static qtd_t ehci_qtd[3]   __attribute__((aligned(32)));
static u8    ehci_buf[512] __attribute__((aligned(4)));
static u8    setup[8]      __attribute__((aligned(4)));
static qh_t  ehci_qh_dummy __attribute__((aligned(32)));

static u32 ehci_op;

static u32 ehci_read(u32 offset) {
  return *(volatile u32*)(ehci_op + offset);
}

static void ehci_write(u32 offset, u32 val) {
  *(volatile u32*)(ehci_op + offset) = val;
}

static void ehci_stop(void) {
  ehci_write(0x00, ehci_read(0x00) & ~1);
  u32 timeout = 100000;
  while (timeout--)
    if (ehci_read(0x04) & (1 << 12)) break;
}

static void ehci_start(void) {
  ehci_write(0x00, ehci_read(0x00) | 1);
  u32 timeout = 100000;
  while (timeout--)
    if (!(ehci_read(0x04) & (1 << 12))) break;
}

static void qtd_init(qtd_t *td, u32 next, u32 token, void *buf, u16 len) {
  td->next     = next;
  td->alt_next = 1;
  td->token    = token | ((u32)len << 16) | (3 << 10); // CErr = 3
  td->buf[0]   = (u32)buf;
  td->buf[1]   = 0;
  td->buf[2]   = 0;
  td->buf[3]   = 0;
  td->buf[4]   = 0;
}

static void ehci_port_reset(void) {
  // порт должен принадлежать EHCI перед сбросом
  u32 portsc = ehci_read(0x44);
  if (portsc & (1 << 13)) {
    portsc &= ~(1 << 13);
    ehci_write(0x44, portsc);
    u32 timeout = 100000;
    while (timeout--);
  }

  portsc = ehci_read(0x44);
  portsc = (portsc & ~0x00E0) | (1 << 8);
  ehci_write(0x44, portsc);

  u32 timeout = 1000000;
  while (timeout--)
    if (!(ehci_read(0x44) & (1 << 8))) break;

  ehci_write(0x44, ehci_read(0x44) & ~(1 << 8));

  // ждём port enabled (бит 2)
  timeout = 1000000;
  while (timeout--)
    if (ehci_read(0x44) & (1 << 2)) break;
}

static void ehci_run_xfer(void) {
  ehci_write(0x04, ehci_read(0x04) | (1 << 4) | (1 << 1));
  ehci_write(0x00, ehci_read(0x00) & ~((1 << 5) | (1 << 4)));

  u32 timeout = 100000;
  while (timeout--)
    if (!(ehci_read(0x04) & (1 << 15))) break;

  ehci_write(0x18, (u32)&ehci_qh_dummy);

  // сначала запускаем контроллер
  ehci_start();
  timeout = 100000;
  while (timeout--)
    if (!(ehci_read(0x04) & (1 << 12))) break;

  // Если компилятор GCC/Clang
  __asm__ __volatile__ ("wbinvd" : : : "memory"); 

  // потом включаем async schedule
  ehci_write(0x00, ehci_read(0x00) | (1 << 5));
  timeout = 1000000;
  while (timeout--)
    if (ehci_read(0x04) & (1 << 15)) break;

  write_string("sts: "); print_hex(ehci_read(0x04)); put_char(space);
  write_string("cmd: "); print_hex(ehci_read(0x00)); put_char(space);
  write_string("td0: "); print_hex(ehci_qtd[0].token); put_char(space);
  write_string("td1: "); print_hex(ehci_qtd[1].token);

  // ждём td1
  timeout = 5000000;
  while (timeout--)
    if (!(ehci_qtd[1].token & (1 << 7))) break;
  if (timeout == 0) write_string("td1 timeout ");
}

static void ehci_get_descriptor(void) {
  setup[0] = 0x80;
  setup[1] = 0x06;
  setup[2] = 0x00;
  setup[3] = 0x01;
  setup[4] = 0x00;
  setup[5] = 0x00;
  setup[6] = 0x12;
  setup[7] = 0x00;

  qtd_init(&ehci_qtd[0], (u32)&ehci_qtd[1],
    (0 << 31) | (2 << 8) | (1 << 7),
    setup, 8);

  qtd_init(&ehci_qtd[1], (u32)&ehci_qtd[2],
    (1 << 31) | (1 << 8) | (1 << 7),
    ehci_buf, 18);

  qtd_init(&ehci_qtd[2], 1,
    (1 << 31) | (0 << 8) | (1 << 7),
    0, 0);

  // dummy QH — голова списка с H-bit
  ehci_qh_dummy.hlp      = (u32)&ehci_qh | 2;
  ehci_qh_dummy.epchar   = (1 << 15) | (1 << 13); // H-bit + High Speed
  ehci_qh_dummy.epcap    = (1 << 29);
  ehci_qh_dummy.cur_qtd  = 0;
  ehci_qh_dummy.next_qtd = 1;
  ehci_qh_dummy.alt_qtd  = 1;
  ehci_qh_dummy.token    = 0;
  ehci_qh_dummy.buf[0]   = 0;
  ehci_qh_dummy.buf[1]   = 0;
  ehci_qh_dummy.buf[2]   = 0;
  ehci_qh_dummy.buf[3]   = 0;
  ehci_qh_dummy.buf[4]   = 0;

  // рабочий QH — без H-bit, указывает обратно на dummy
  ehci_qh.hlp      = (u32)&ehci_qh_dummy | 2;
  ehci_qh.epchar   = (0 << 0)  |
                     (0 << 8)  |
                     (1 << 13) |
                     (1 << 14) |
                     (64 << 16); // без H-bit!
  ehci_qh.epcap    = (1 << 29);
  ehci_qh.cur_qtd  = 0;
  ehci_qh.next_qtd = (u32)&ehci_qtd[0];
  ehci_qh.alt_qtd  = 1;
  ehci_qh.token    = 0;
  ehci_qh.buf[0]   = 0;
  ehci_qh.buf[1]   = 0;
  ehci_qh.buf[2]   = 0;
  ehci_qh.buf[3]   = 0;
  ehci_qh.buf[4]   = 0;

  print_hex(ehci_read(0x44));
  write_string("td1 tok before: "); print_hex(ehci_qtd[1].token);

  ehci_run_xfer();

  if (ehci_read(0x04) & (1 << 1)) write_string("USB Error Interrupt ");
  if (ehci_read(0x04) & (1 << 4)) write_string("Host System Error ");
  if (ehci_qtd[1].token & (1 << 7)) { write_string("Controller Ignore in 1"); put_char(space); }
  for (int i = 0; i < 3; i++) {
    if (ehci_qtd[i].token & (1 << 3)) { write_string("Transaction Error in "); print_hex(i); put_char(space); }
    if (ehci_qtd[i].token & (1 << 4)) { write_string("Babble Detected in "); print_hex(i); put_char(space); }
    if (ehci_qtd[i].token & (1 << 5)) { write_string("Data Buffer Error in "); print_hex(i); put_char(space); }
  }
  write_string("desc: ");
  for (int i = 0; i < 18; i++) {
    print_hex(ehci_buf[i]);
    put_char(space);
  }
}

void ehci_init(void) {
  // останавливаем companion UHCI контроллеры
  for (u16 b = 0; b < 256; b++)
  for (u16 s = 0; s < 32; s++)
  for (u8 f = 0; f < 8; f++) {
    u32 val = pci_read(b, s, f, 0x08);
    u8 class    = (val >> 24) & 0xFF;
    u8 subclass = (val >> 16) & 0xFF;
    u8 progif   = (val >>  8) & 0xFF;
    if (class == 0x0C && subclass == 0x03 && progif == 0x00) {
      u32 uhci_bar = pci_read(b, s, f, 0x20) & ~0x3;
      outw(uhci_bar + 0, 0x0002); // UHCI CMD: Host Controller Reset
    }
  }

  u16 bus, slot;
  u8 func = 0;
  u32 bar0 = 0;

  for (bus = 0; bus < 256; bus++)
  for (slot = 0; slot < 32; slot++)
  for (func = 0; func < 8; func++) {
    u32 val = pci_read(bus, slot, func, 0x08);
    u8 class    = (val >> 24) & 0xFF;
    u8 subclass = (val >> 16) & 0xFF;
    u8 progif   = (val >>  8) & 0xFF;
    if (class == 0x0C && subclass == 0x03 && progif == 0x20) {
      u32 b0 = pci_read(bus, slot, func, 0x10) & ~0xF;
      u8 capl = *(volatile u8*)b0;
      u32 op = b0 + capl;
      u32 ps = *(volatile u32*)(op + 0x44);
      if (ps & 1) { // Connected
        write_string("slot: "); print_dec(slot);
        bar0 = b0;
        u32 pci_cmd = pci_read(bus, slot, func, 0x04);
        pci_write(bus, slot, func, 0x04, pci_cmd | 0x06); // 0x04 - BM, 0x02 - Memory Space
        goto found;
      }
    }
  }

  write_string("USB not found");
  return;

found:
  ehci_op = bar0 + (*(volatile u8*)bar0);

  // handoff от BIOS
  u32 hccparams = *(volatile u32*)(bar0 + 0x08);
  u8 eecp = (hccparams >> 8) & 0xFF;
  if (eecp >= 0x40) {
    u32 usblegsup = pci_read(bus, slot, func, eecp);
    usblegsup |= (1 << 24);
    pci_write(bus, slot, func, eecp, usblegsup);
    u32 timeout = 1000000;
    while (timeout--)
      if (!(pci_read(bus, slot, func, eecp) & (1 << 16))) break;
  }

  ehci_write(0x10, 0);        // CTRLDSSEGMENT = 0
  ehci_write(0x08, 0);        // USBINTR = 0
  ehci_write(0x14, 0);        // PERIODICLISTBASE = 0
  ehci_start();               // Run/Stop = 1 — сначала запускаем!
  ehci_write(0x40, 1);        // CONFIGFLAG = 1 — потом переключаем порты

  u32 delay = 1000000;
  while (delay--);

  ehci_port_reset();
  ehci_get_descriptor();
}