/*-
 * Copyright (c) 2015 Thomas Haggett
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THOMAS HAGGETT ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <xhyve/firmware/multiboot.h>
#include <xhyve/vmm/vmm_api.h>

#define MULTIBOOT_MAGIC 0x1BADB002
#define MULTIBOOT_SEARCH_END 0x2000


struct multiboot_header {
  uint32_t magic;
  uint32_t flags;
  uint32_t checksum;
};

struct multiboot_mem_header {
  uint32_t header_addr;
  uint32_t load_addr;
  uint32_t load_end_addr;
  uint32_t bss_end_addr;
  uint32_t entry_addr;
};

struct multiboot_info  {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  uint32_t cmdline_addr;
  uint32_t mods_count;
  uint32_t mods_addr;
} ;

static FILE *kernel;
static struct multiboot_header header;
static struct multiboot_mem_header mem_header;

// static long load_offset;

static unsigned long image_offset;
static unsigned long image_load;
static unsigned long image_header;

static unsigned long image_length;

static unsigned long physical_load;
static unsigned long physical_header;

static char* boot_cmdline;

void multiboot_init(char *kernel_path, char *module_spec, char *cmdline) {
  printf("multiboot_init(%s, %s, %s)\n", kernel_path, module_spec, cmdline);
  kernel = fopen(kernel_path, "r");

  uint8_t found = 0;
  boot_cmdline = cmdline;

  fseek(kernel, 0L, SEEK_SET);

  while(!found || !feof(kernel) || ftell(kernel) < MULTIBOOT_SEARCH_END) {
    if( 1 != fread((void*)&header.magic, sizeof(uint32_t), 1, kernel))
      goto fail;

    if(header.magic == MULTIBOOT_MAGIC) {
      if( 1 != fread((void*)&header.flags, sizeof(uint32_t), 1, kernel))
        goto fail;

      if( 1 != fread((void*)&header.checksum, sizeof(uint32_t), 1, kernel))
        goto fail;

      if(header.checksum + header.flags + header.magic == 0 ){
        break;
      }

      // make sure to jump 64-bits back in the file, so that if there happens to be two
      // magic values one after another, it doesn't get ignored!
      fseek(kernel, -8, SEEK_CUR);

    }

    memset(&header, 0x0, sizeof(header));
  }

  if( header.magic == MULTIBOOT_MAGIC ) {
    image_header = (unsigned long) (ftell(kernel) - 12);
    printf("Header found at %lx!\n", image_header);

    // read in the memory placement header if it's set
    if(header.flags & (1<<16)) {
      printf("Loading memory header\n");
      if( 1 != fread((void*)&mem_header, sizeof(mem_header), 1, kernel) )
        goto fail;

      printf(" header_addr=%x\n", mem_header.header_addr);
      printf(" load_addr=%x\n", mem_header.load_addr);
      printf(" load_end_addr=%x\n", mem_header.load_end_addr);
      printf(" bss_end_addr=%x\n", mem_header.bss_end_addr);
      printf(" entry_addr=%x\n", mem_header.entry_addr);

      // we start loading from (header_addr-load_addr)
      printf("Load starts from %x\n", mem_header.header_addr - mem_header.load_addr);
      printf("Load into memory at %x\n", mem_header.header_addr);
      printf("Entry address is %x\n", mem_header.entry_addr);

      // work out the load_end_addr from the file length if it isn't specified
      if( mem_header.load_end_addr == 0 ) {
        image_offset = mem_header.header_addr - image_header;
        image_load = mem_header.load_addr - image_offset;

        // work out the lenght of the file
        fseek(kernel, 0, SEEK_END);
        image_length = (unsigned long)(ftell(kernel) - (long)image_load);

        printf("image_length is %li\n", image_length);

        mem_header.load_end_addr = (uint32_t)image_length + mem_header.load_addr;
        printf("Load end addr is %x\n", mem_header.load_end_addr);

        physical_load = mem_header.load_addr;
        physical_header = mem_header.header_addr;
      } else {
        image_length = mem_header.load_end_addr - mem_header.load_addr;
      }
    }
  }

  return;
fail:
  printf("Failed to load multiboot kernel!\n");
  exit(1);
}



uint64_t multiboot(void) {

  printf("multiboot();\n");
  void *gpa_map, *gpa_end;

  gpa_map = xh_vm_map_gpa(0, xh_vm_get_lowmem_size());
  gpa_end = (void *)((uintptr_t)gpa_map + xh_vm_get_lowmem_size());

  // read the file into the correct place!
  void* host_load_addr = (void*)((uintptr_t)gpa_map + (uint32_t)physical_load);
  printf("Loading kernel from %lx -> %lx (%li bytes):\n", image_load, image_load+image_length, image_length);
  printf("               into %lx -> %lx\n", (uintptr_t)gpa_map + physical_load, (uintptr_t)gpa_map + physical_load + image_length);
  printf("               into %lx -> %lx\n", physical_load, physical_load + image_length);

  fseek(kernel, (long)image_load, SEEK_SET);
  if( 1 != fread(host_load_addr, image_length, 1, kernel))
    goto fail;
  printf("Done!\n");

  // write out the multiboot info struct
  void* p = (char*)((uintptr_t)host_load_addr + image_length);
  struct multiboot_info* mb_info = (struct multiboot_info*)p;
  p = (void*) ((uintptr_t)p +  sizeof(struct multiboot_info));
  mb_info->flags = 0x0;

  if( boot_cmdline ) {
    strcpy((char*)p, boot_cmdline);

    mb_info->flags |= (1<<2);
    mb_info->cmdline_addr = (uint32_t)((uintptr_t)p - (uintptr_t)gpa_map);
    printf("cmdline addr is %x\n", mb_info->cmdline_addr);
    p = (void*) ((uintptr_t)p +  strlen(boot_cmdline) + 1);
  }

  mb_info->flags |= (1<<0);
  mb_info->mem_lower = (uint32_t)640*1024;
  mb_info->mem_upper = (uint32_t)xh_vm_get_lowmem_size();

  xh_vcpu_reset(0);

  
  xh_vm_set_register(0, VM_REG_GUEST_CR0, 0x21);
  xh_vm_set_register(0, VM_REG_GUEST_RAX, 0x2BADB002);
  xh_vm_set_register(0, VM_REG_GUEST_RBX, (uintptr_t)mb_info - (uintptr_t)gpa_map);
  xh_vm_set_register(0, VM_REG_GUEST_RIP, mem_header.entry_addr);

  // xh_vm_set_desc(0, VM_REG_GUEST_GDTR, BASE_GDT, 0x1f, 0);
  xh_vm_set_desc(0, VM_REG_GUEST_CS, 0, 0xffffffff, 0xc09b);
  xh_vm_set_desc(0, VM_REG_GUEST_DS, 0, 0xffffffff, 0xc093);
  xh_vm_set_desc(0, VM_REG_GUEST_ES, 0, 0xffffffff, 0xc093);
  xh_vm_set_desc(0, VM_REG_GUEST_SS, 0, 0xffffffff, 0xc093);
  xh_vm_set_register(0, VM_REG_GUEST_CS, 0x10);
  xh_vm_set_register(0, VM_REG_GUEST_DS, 0x18);
  xh_vm_set_register(0, VM_REG_GUEST_ES, 0x18);
  xh_vm_set_register(0, VM_REG_GUEST_SS, 0x18);
  
  // xh_vm_set_register(0, VM_REG_GUEST_RBP, 0);
  // xh_vm_set_register(0, VM_REG_GUEST_RDI, 0);
  // xh_vm_set_register(0, VM_REG_GUEST_RFLAGS, 0x2);
  // xh_vm_set_register(0, VM_REG_GUEST_RSI, BASE_ZEROPAGE);
  
  return mem_header.entry_addr;

fail:
  printf("Failed to load kernel!\n");
  exit(1);
}
