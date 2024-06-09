#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"

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

/**
 * @brief 
*/
uint64 sys_mmap(void) {
  uint64 addr;
  uint64 length;
  int prot;
  int flags;
  int fd;
  uint64 offset;
  if (argaddr(0, &addr) < 0 || argaddr(1, &length) < 0 || argint(2, &prot) < 0 ||
      argint(3, &flags) < 0 || argint(4, &fd) < 0 || argaddr(5, &offset)) {
    return -1;
  }

  struct vma *vma = vma_new();
  if (vma == 0) {
    return -1;
  }

  // PROTECTION check
  struct proc *proc = myproc();
  struct file *file = proc->ofile[fd];

  if ((prot & PROT_WRITE) && (!file->writable) && (flags & MAP_SHARED)) {
    vma_free(vma);
    return -1;
  }

  vma_init(vma, myproc(), 0, length, prot, flags, file, offset);

  return vma->start;
}

uint64 sys_munmap(void) {
  // region is at the start or end, or the whole region.
  // but not in the middle

  uint64 addr;
  uint64 length;

  if (argaddr(0, &addr) < 0 || argaddr(1, &length) < 0) {
    return -1;
  }

  struct vma *vma = vma_find(addr);
  if (vma == 0) {
    return -1;
  }

  vma_unmap_multi(vma, addr, length);

  // decrease cnt of file if the whole region has been unmap, 
  // release it
  if (vma->length == 0) {
    vma_free(vma);
    // fileclose(vma->file);
    // memset(vma, 0, sizeof(vma));
  }

  return 0;
}
