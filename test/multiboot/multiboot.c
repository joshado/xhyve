#include <stdint.h>
#include <stdarg.h>

void main(uint32_t eax, uint32_t ebx);

asm (
  "start:\n\t"
  "multiboot_header:\n\t"
  ".p2align 2\n\t"
  ".long 0x1BADB002\n\t"
  ".long 0x10003\n\t"
  ".long 0xe4514ffb\n\t"
  ".long   multiboot_header\n\t"  // header_addr
  ".long   start\n\t"             // load_addr
  ".long   0\n\t"                 // load_end_addr
  ".long   0\n\t"                 // bss_end_addr
  ".long   _multiboot_entry\n\t"   // entry_addr
  "_multiboot_entry:\n\t"
  "movl    $(stack + 0x4000), %esp\n\t"
  "pushl   $0\n\t"
  "popf\n\t"
  "pushl %ebx\n\t"
  "pushl %eax\n\t"
  "call     _main\n\t"
  "hlt\n\t"
  "stack:"
  ".skip 0x4000"
);

struct multiboot_info {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  char* cmdline_addr;
  uint32_t mods_count;
  uint32_t mods_addr;
};


static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile ( "inb %[port], %[ret]" : [ret] "=a"(ret) : [port] "Nd"(port) );
    return ret;
}

static inline uint64_t rdtsc()
{
    uint64_t ret;
    asm volatile ( "rdtsc" : "=A"(ret) );
    return ret;
}

void sleep(int count) 
{
  uint64_t entry = rdtsc();
  while(rdtsc() < entry + count) { }
}

#define VIDEO                   0xB8000
#define COLUMNS                 80
#define LINES                   24

#define COM1  0x3f8

void putchar(char v) 
{
  // wait for the transmit buffer to empty
  while((inb(COM1 + 5) & 0x20) == 0 ) { }

  // write the value!
  outb(COM1, v);
}

void memset(char *buffer, char value, int count) {
  while(count > 0) {
    buffer[count--] = value;
  }
}

char* atoi(uint32_t value, uint8_t d) {
  static char buf[32];
  char *p = buf+sizeof(buf);
  *p-- = 0x0;

  if(d == 'x') d = 16;
  if(d == 'i') d = 10;

  do {
    int r = value % d;
    *p-- = ((r < 10) ? r + '0' : r - 10 + 'a');
  } while(value /= d);
  return (p + 1);
}
void putstring(char* string) {
  while(*string != 0x0) putchar(*string++);
}

void printf(char* format, ...) {
  va_list ap;
  va_start(ap, format);

  char value;

  while((value  = *format++) != 0x0) {
    if(value == '%') {
      value = *format++;
      switch(value) {
      case 'x':
      case 'i':
        putstring(atoi(va_arg(ap, int), value));
        break;
      case 's':
        putstring(va_arg(ap, char*));
        break;
      default:
        putchar('?');
        break;
      }
    } else {
      putchar(value);
    }
  }
}


struct multiboot_mod_list
{
  uint32_t mod_start;
  uint32_t mod_end;
  uint32_t cmdline;
  uint32_t pad;
};
typedef struct multiboot_mod_list multiboot_module_t;

int parse_multiboot_structure(void* p) {
  struct multiboot_info* h = (struct multiboot_info*)p;
  
  printf("    - flags are 0x%x\n", h->flags);

  if( h->flags & (1<<0)) {
    printf("    - low-memory:   %i\n", h->mem_lower);
    printf("    - upper-memory: %i\n", h->mem_upper);  
  }
  if( h->flags & (1<<2)) {
    printf("    - command_line is '%s'\n", h->cmdline_addr);
  }
  if( h->flags & (1<<4)) {
    printf("    - module count: %i\n", h->mods_count);
    printf("    - module addr: %x\n", h->mods_addr);
    multiboot_module_t *mod;
    uint32_t i;

    for (i = 0, mod = (multiboot_module_t *) h->mods_addr; i < h->mods_count; i++, mod++) {
      char* p = (char*)mod->mod_start;
      printf("     > mod_start = 0x%x, mod_end = 0x%x, cmdline = '%s'\n", (unsigned) mod->mod_start, (unsigned) mod->mod_end, (char *) mod->cmdline);
      printf("     | %x %x %x %x %x %x %x %x\n", *p++, *p++, *p++, *p++, *p++, *p++, *p++, *p++);
      printf("     | ...\n");
      p =(char*) mod->mod_end - 8;
      printf("     | %x %x %x %x %x %x %x %x\n", *p++, *p++, *p++, *p++, *p++, *p++, *p++, *p++);
    }
  }
  return 0;
}

void main(uint32_t eax, uint32_t ebx) 
{
  printf("\n");
  printf("Checking machine state:\n");
  printf(" * EAX must contain 0x2BADB002\n");
  if( eax != 0x2BADB002 ) goto failed;

  printf(" * EBX must contain pointer to multiboot info\n");
  if( parse_multiboot_structure((void*)ebx) ) goto failed;

  // printf("\nTODO:\n");
  // printf(" * CS must be 32-bit read/execute code segment with offset 0 / limit 0xFFFFFFFF\n");
  // printf(" * DS must be 32-bit read/write data segment with offset 0 / limit 0xFFFFFFFF\n");
  // printf(" * ES must be 32-bit read/write data segment with offset 0 / limit 0xFFFFFFFF\n");
  // printf(" * FS must be 32-bit read/write data segment with offset 0 / limit 0xFFFFFFFF\n");
  // printf(" * GS must be 32-bit read/write data segment with offset 0 / limit 0xFFFFFFFF\n");
  // printf(" * SS must be 32-bit read/write data segment with offset 0 / limit 0xFFFFFFFF\n");
  printf(" * A20 gate must be enabled\n");
  printf("    - %x\n", inb(0x92));
  // printf(" * CR0 - bit 31 must be cleared, bit 0 must be set\n");
  // printf(" * EFLAGS - bit 17 must be cleared, bit 9 must be cleared\n");
  // printf("\n");
  // printf("Looking for multiboot header\n");
  printf("SUCH PASS!\n");
  return;
failed:
  printf("     ^^ fail! :-(\n");
}
