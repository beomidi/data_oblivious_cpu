#ifndef _NOUARCH_H_
#define _NOUARCH_H_

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/resource.h>

#include "context.h"
#include "../ptedit/ptedit.h"

#ifndef PARANOID
#define PARANOID 0
#endif

#define DEBUG //printf

#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    size_t        e_entry;
    size_t        e_phoff;
    size_t        e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} ElfN_Ehdr;

typedef struct {
    uint32_t   sh_name;
    uint32_t   sh_type;
    uint64_t   sh_flags;
    size_t     sh_addr;
    size_t     sh_offset;
    uint64_t   sh_size;
    uint32_t   sh_link;
    uint32_t   sh_info;
    uint64_t   sh_addralign;
    uint64_t   sh_entsize;
} Elf64_Shdr;

void* recallocarray(void* addr, size_t oldnmemb, size_t nmemb, size_t size);

#if PARANOID
#include <signal.h>
#include <setjmp.h>

static jmp_buf specfence_buf;

void unblock_signal(int signum __attribute__((__unused__))) {
  sigset_t sigs;
  sigemptyset(&sigs);
  sigaddset(&sigs, signum);
  sigprocmask(SIG_UNBLOCK, &sigs, NULL);
}

// ---------------------------------------------------------------------------
void specfence_segfault_handler(int signum) {
  (void)signum;
  unblock_signal(SIGSEGV);
  longjmp(specfence_buf, 1);
}

#endif

uint64_t nouarchrdtsc() {
  uint64_t a, d;
  __asm__ volatile("fence");
  asm volatile ("csrrs %0, 0xc00, x0" : "=r" (a) );
  __asm__ volatile("fence");
  return a;
}

#ifndef NOUARCH_DISABLE
void __attribute__((section(".specfence"), constructor)) init_nouarch() {
    size_t start = nouarchrdtsc();
    size_t stack = 0;
    FILE* f = fopen("/proc/self/exe", "rb");
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* elf = (char*)malloc(fsize);
    (void)fread(elf, 1, fsize, f);
    fclose(f);

    ptedit_init();

    ElfN_Ehdr* hdr = (ElfN_Ehdr*)elf;

    Elf64_Shdr* shdr = (Elf64_Shdr*)(elf + hdr->e_shoff);
    Elf64_Shdr symtab = shdr[hdr->e_shstrndx];

    // get relocation offset
    size_t offset = 0;
    int i;
    for(i = 0; i < hdr->e_shnum; i++) {
        if(strcmp((char*)(elf + symtab.sh_offset + shdr[i].sh_name), ".specfence") == 0) {
                DEBUG("init_nouarch @ %zx / %zx\n", shdr[i].sh_addr, (size_t)init_nouarch);
                offset = (size_t)init_nouarch - shdr[i].sh_addr;
        }
    }

    // set "secret" section to uncachable
    for(i = 0; i < hdr->e_shnum; i++) {
        if(strcmp((char*)(elf + symtab.sh_offset + shdr[i].sh_name), ".secret") == 0) {
            DEBUG("Secret @ %zx (len: %zd)\n", shdr[i].sh_addr + offset, shdr[i].sh_size);
            int j;
            for(j = 0; j < (shdr[i].sh_size + 4095) / 4096; j++) {
                void* addr = (void*)(shdr[i].sh_addr + offset + j * 4096);
                *(volatile char*)addr;
                DEBUG(" - Setting %p to uncachable\n", addr);
                ptedit_entry_t entry = ptedit_resolve(addr, 0);
                entry.pte = ptedit_apply_nouarch(entry.pte);
                entry.valid = PTEDIT_VALID_MASK_PTE;
                ptedit_update(addr, 0, &entry);
                ptedit_entry_t entry_test = ptedit_resolve(addr, 0);
            }
        }
    }

#if PARANOID
    size_t stack_start = (size_t)&stack & ~0xfff;
    struct rlimit rlim;
    getrlimit(RLIMIT_STACK, &rlim);
    size_t stack_size = (size_t)rlim.rlim_cur;
    DEBUG("start: %zx, size: %zd\n", stack_start, stack_size / 1024);
    signal(SIGSEGV, specfence_segfault_handler);
    if (!setjmp(specfence_buf)) {
    	int i;
        for(i = 0; i < stack_size / 4096; i++) {
            void* addr = (void*)(stack_start - i * 4096);
            DEBUG("%p\n", addr);
            *(volatile char*)addr;
            ptedit_entry_t entry = ptedit_resolve(addr, 0);
            if((entry.valid & PTEDIT_VALID_MASK_PTE) && entry.pte & (1 << PTEDIT_PAGE_BIT_PRESENT)) {
                entry.pte = ptedit_apply_nouarch(entry.pte);
                entry.valid = PTEDIT_VALID_MASK_PTE;
                ptedit_update(addr, 0, &entry);
                DEBUG(" -> UC\n");
            }
        }
    }
    signal(SIGSEGV, SIG_DFL);
#endif

    printf("[context] active, took %zd cycles\n", nouarchrdtsc() - start);

    ptedit_cleanup();
}

static int _nouarch_secure_heap = 0;
void nouarch_secure_heap(int secure) {
    _nouarch_secure_heap = secure;
}

void nouarch_set(void* addr, size_t len) {
  size_t addr_aligned = (size_t)addr;
  size_t first = addr_aligned & ~0xfff;
  size_t last = (addr_aligned + len + 4095) & ~0xfff;
  ptedit_init();
  size_t j;
  for(j = first; j < last; j += 4096) {
    *(volatile char*)j;

    DEBUG(" - Setting %p to uncachable\n", j);
    ptedit_entry_t entry = ptedit_resolve(j, 0);
    entry.pte = ptedit_apply_nouarch(entry.pte);
    entry.valid = PTEDIT_VALID_MASK_PTE;
    ptedit_update(j, 0, &entry);
  }
  ptedit_cleanup();
}

void* malloc_nouarch(size_t len) {
    int uc_mt;
    if(_nouarch_secure_heap) {
        ptedit_init();
       /* uc_mt = ptedit_find_first_mt(PTEDIT_MT_UC);
        if(uc_mt == -1) {
            printf("[ERROR] Uncachable not supported on this kernel/machine\n");
            ptedit_cleanup();
        }*/
    }
    size_t len_aligned = ((len + 4095) / 4096) * 4096;
    void* mem = mmap(0, len_aligned + 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if(_nouarch_secure_heap) {
    	int j;
        for(j = 1; j < len_aligned / 4096 + 1; j++) {
            DEBUG(" - Setting %p to uncachable\n", mem + j * 4096);
            ptedit_entry_t entry = ptedit_resolve(mem + j * 4096, 0);
            entry.pte = ptedit_apply_nouarch(entry.pte);
            entry.valid = PTEDIT_VALID_MASK_PTE;
            ptedit_update(mem + j * 4096, 0, &entry);
        }
        ptedit_cleanup();
    }
    *(size_t*)mem = len;
    *(((size_t*)mem) + 1) = 0xDEAD000E;
    return mem + 4096;
}

void* secure_malloc(size_t len) {
  nouarch_secure_heap(1);
  void *mem = malloc(len);
  nouarch_secure_heap(0);

  return mem;
}

void free_nouarch(void* addr) {
#ifdef OVERWRITE_MALLOC
  if(!addr)
    return;
#endif
  if(*(size_t*)((char*)addr - 4096 + sizeof(size_t)) == 0xDEAD000E) {
    munmap((char*)addr - 4096, *(size_t*)((char*)addr - 4096));
  } else {
#ifndef OVERWRITE_MALLOC
    free(addr);
#endif
  }
}

void* calloc_nouarch(size_t nmemb, size_t size) {
    void* res = malloc_nouarch(nmemb * size);
    memset(res, 0, nmemb * size);
    return res;
}

void* realloc_nouarch(void* addr, size_t size) {
    if(*(size_t*)((char*)addr - 4096 + sizeof(size_t)) == 0xDEAD000E) {
        size_t oldsize = *(size_t*)((char*)addr - 4096);
        if(size < oldsize) return addr;
        void* naddr = malloc_nouarch(size);
        memcpy(naddr, addr, oldsize <= size ? oldsize : size);
        free_nouarch(addr);
        return naddr;
    } else {
        return realloc(addr, size);
    }
}

void* reallocarray_nouarch(void* addr, size_t nmemb, size_t size) {
    return realloc_nouarch(addr, nmemb * size);
}

void* recallocarray_nouarch(void* addr, size_t oldnmemb, size_t nmemb, size_t size) {
        if(nmemb < oldnmemb) return addr;

        void* naddr = calloc_nouarch(size * nmemb, 1);
        memcpy(naddr, addr, ((oldnmemb <= nmemb) ? oldnmemb : nmemb) * size);
        free_nouarch(addr);
        if(nmemb > oldnmemb) memset(naddr + size * oldnmemb, 0, size * (nmemb - oldnmemb));
        return naddr;
}
#endif

#endif



