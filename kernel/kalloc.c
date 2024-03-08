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

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

// PHYSTOP =  0x80000000L + 128 * 1024 * 1024
// MAX_PHYSICS_PAGE_NUM = (PHYSTOP - KENBASE) / 4096 is bigger than actually number of physics page
// Actually used physics page num is (PHYSTOP - PGROUNDUP(end)) / PGSIZE
#define MAX_PHYSICS_PAGE_NUM (128 * 1024 * 1024 / 4096) 

static uint8 physics_references[MAX_PHYSICS_PAGE_NUM];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void initialize_reference(uint64 start, uint64 end) {
  memset(physics_references, 1, sizeof(physics_references));
}

static inline int is_valid_addr(uint64 pa) {
  if ((char *)pa < end || (pa - KERNBASE) / PGSIZE >= MAX_PHYSICS_PAGE_NUM) {
    return 0;
  }
  return 1;
}

void change_reference(uint64 pa, int delta) {
  if (!is_valid_addr(pa)) {
    panic("change_reference");
  }
  uint64 loc = (pa - KERNBASE) / PGSIZE;
  physics_references[loc] = physics_references[loc] + delta;
}

uint8 get_reference(uint64 pa) {
  if (!is_valid_addr(pa)) {
    return -1;
  }
  uint64 loc = (pa - KERNBASE) / PGSIZE;
  return physics_references[loc];
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initialize_reference((uint64)end, PHYSTOP);
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  if (get_reference((uint64)pa) == 0) {
    printf("pa ref:%d\n", get_reference((uint64)pa));
    panic("kfree: free a page had been freed");
  }

  // reference of physics page minus 1
  change_reference((uint64)pa, -1);
  if (get_reference((uint64)pa) == 0) {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    struct run *r = (struct run*)pa;

    // only add page to free list it reference is 0
    r->next = kmem.freelist;
    kmem.freelist = r;
  } 
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;

  if (r) {
    if (get_reference((uint64)r) != 0) {
      panic("something bad happens when controling reference of physics page");
    }

    change_reference((uint64)r, 1);
  }

  release(&kmem.lock);
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
