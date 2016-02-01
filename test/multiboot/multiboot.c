
void main(void);

asm (
  "start:\n\t"
  "multiboot_header:\n\t"
  ".balign 4\n\t"
  ".long 0x1BADB002\n\t"
  ".long 0x10003\n\t"
  ".long 0xe4514ffb\n\t"
  ".long   multiboot_header\n\t"  // header_addr
  ".long   start\n\t"             // load_addr
  ".long   0\n\t"                 // load_end_addr
  ".long   0\n\t"                 // bss_end_addr
  ".long   _main\n\t"   // entry_addr
  ".space 65536\n\t"
);


#include <stdint.h>

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

static volatile uint8_t *video;


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

void com_write(char v) 
{
  // wait for the transmit buffer to empty
  while((inb(COM1 + 5) & 0x20) == 0 ) { }

    // write the value!
    outb(COM1, v);
}

void com_write_string(char* string) 
{
  while(*string != 0x0)
    com_write(*string++);
}


void main() 
{
  int i = 0;
  char v = 0;

  com_write_string("\n");
  com_write_string("Booting tinykernel!\n");

  while(1) {
    video = (unsigned char *) VIDEO;

    for (i = 0; i < COLUMNS * LINES * 2; i++) {
      *(video + i) = v++;

    }
    v++;
    com_write_string("Loop!\n");

    sleep(100000000);
  }
}
