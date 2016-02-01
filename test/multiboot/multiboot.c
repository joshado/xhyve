
void main(void);

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
  "call     _main\n\t"
  "hlt\n\t"
  "stack:"
  ".skip 0x4000"
);

#include <stdint.h>
#include <stdarg.h>

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

// static volatile uint8_t *video;


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

void main() 
{




}
