

void multiboot_entry(void);

void main(void) {
  asm (
    "start:"
    ".align 8\n\t"
    "multiboot_header:"
    ".long 0x1BADB002\n\t"
    ".long 0x10003\n\t"
    ".long 0xe4514ffb\n\t"
    ".long   multiboot_header\n\t"  // header_addr
    ".long   start\n\t"            // load_addr
    ".long   0\n\t"                 // load_end_addr
    ".long   0\n\t"                 // bss_end_addr
    ".long   _multiboot_entry\n\t"   // entry_addr
  );

  multiboot_entry();
  while(1) { }
}

#define VIDEO                   0xB8000
#define COLUMNS                 80
#define LINES                   24
#define ATTRIBUTE               7

static volatile unsigned char *video;

typedef unsigned long long int uint64_t;

static inline uint64_t rdtsc()
{
    uint64_t ret;
    asm volatile ( "rdtsc" : "=A"(ret) );
    return ret;
}

void sleep(int count) {
  uint64_t entry = rdtsc();
  while(rdtsc() < entry + count) { }
}

void multiboot_entry() {
  int i = 0;
  char v = 0;

  while(1) {
    video = (unsigned char *) VIDEO;

    for (i = 0; i < COLUMNS * LINES * 2; i++) {
      *(video + i) = v++;

    }
    v++;


    sleep(100000000);
  }
}