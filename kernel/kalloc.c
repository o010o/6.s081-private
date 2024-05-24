// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
void *kalloc_from_mem_pool(int id);
void add_to_mem_pool(int cpuid, void *pa);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmems[NCPU];

void
kinit()
{
  char name[32];
  for (int i = 0; i < NCPU; ++i) {
    snprintf(name, sizeof(name), "kmem-%d", i);
    initlock(&kmems[i].lock, name);
  }

  freerange(end, (void*)PHYSTOP);
}

void add_to_mem_pool(int cpuid, void *pa) {
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmems[cpuid].lock);
  r->next = kmems[cpuid].freelist;
  kmems[cpuid].freelist = r;
  release(&kmems[cpuid].lock);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);

  for(int i = 0; p + PGSIZE <= (char*)pa_end; p += PGSIZE, ++i) {
    add_to_mem_pool(i % NCPU, p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  push_off();
  int id = cpuid();
  pop_off();

  add_to_mem_pool(id, pa);
}

void *kalloc_from_mem_pool(int id) {
  struct run *r;

  acquire(&kmems[id].lock);
  r = kmems[id].freelist;
  if(r)
    kmems[id].freelist = r->next;
  release(&kmems[id].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int id = cpuid();
  pop_off();

  for (int i = 0; i < NCPU; ++i) {
    int t_id = (id + i) % NCPU;
    r = kalloc_from_mem_pool(t_id);
    if (r) {
      break;
    }
  }

  return (void*)r;
}
