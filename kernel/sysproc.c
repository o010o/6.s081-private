#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL

static inline void set_mask(char *mask, int loc) {
  // mask[loc / 8] = mask[loc / 8] | (1 << (7 - loc % 8));
  mask[loc >> 3] = mask[loc >> 3] | (1 << (loc & 0x7));
}

// max number of ptes could check
#define MAX_CHECK_NUM (1024 >> 3)

// copy from vm.c
extern pte_t * walk(pagetable_t pagetable, uint64 va, int alloc);

int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  // arg 1: start virtual address
  // arg 2: check number of pages
  // arg 3: mask whick store check result, and the least significant bit corresponds to the first page
  char mask[MAX_CHECK_NUM];
  uint64 va;
  int len;
  char *output;
  pte_t *pte;
  pagetable_t pagetable = myproc()->pagetable;

  memset(mask, 0, sizeof(mask));
  
  // read args, using argaddr argint
  if (argaddr(0, &va) < 0) {
    return -1;
  }

  if (argint(1, &len) < 0) {
    return -1;
  }

  if (argaddr(2, (uint64 *)&output) < 0) {
    return -1;
  }
  
  // use walk to check e ptes which meets condition
  for (int i = 0; i < len; ++i) {
    pte = walk(pagetable, va, 0);
    va += PGSIZE;
    if (pte == 0) {
      continue; 
    }
    if ((PTE_FLAGS(*pte) & PTE_A) && (PTE_FLAGS(*pte) & PTE_V)) {
      // clear pte_a after access, otherwise it could extist forever
      *pte = *pte & ~(PTE_A);
      set_mask(mask, i);
    }
  }

  // write check result to output buffer
  if (copyout(pagetable, (uint64)output, mask, (len + 7) / 8) < 0) {
    return -1;
  }

  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
